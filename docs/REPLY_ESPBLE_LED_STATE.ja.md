# 返答: EspBle HID Device LED 状態 getter の追加依頼

宛先: EspUsbDevice / ESP32KeyBridge
対象依頼: EspUsbDevice `docs/ESPBLE_LED_STATE_REQUEST.ja.md`

この文書は提出後に削除します。確定した判断は[DECISIONS.ja.md](DECISIONS.ja.md)（HID 23〜27）と[HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md)にあります。

## 結論: 対応済み。ただし戻り値の型が依頼と異なります

依頼の 3 項目と「listener 化しない」「命名」はすべて受けました。**戻り値だけ `const EspBleHidKeyboardOutputReport &` ではなく値返しにしています。** 理由は下記のとおりで、これは据え置きます。EspUsbDevice 側を値返しへ変える必要はありません。

```cpp
class EspBleHidKeyboard {
public:
  EspBleHidKeyboardOutputReport ledState() const;   // ← 依頼は const & だった
};
```

## 参照を返せない理由

**EspBle では LED 状態が別 task から書かれます。**

Host が LED Output Report を書くと、NimBLE の GATT access callback が **stack task 上で**呼ばれ、そこで最新値を保存します。一方 `ledState()` を呼ぶのはアプリの loop task です。保存先は `impl_->mutex` で保護しているので、**参照を返すと呼び出し側がロック外で他 task が書き換える実体を読む**ことになり、データ競合になります。ロック内でコピーして値を返すのが唯一安全な形です。

EspBle 内でも扱いが分かれています。

| getter | 戻り値 | 書き込み元 |
|---|---|---|
| `heldState()`（NKRO 保持状態） | `const &` | 送信経路のみ = 呼び出し側 task |
| `ledState()` | 値 | GATT access callback = stack task |

`heldState()` は参照のままです。同じ判断を機械的に適用したのではなく、**書き込み元の task が違う**ことによる差です。

USB 側で参照が成立するのは、`onHidSetReport()` の実行 context がこの制約を持たないためだと理解しています。そちらの実装を変える理由にはならないと考えています。

利用側から見た差は `const auto &led = kb.ledState();` が使えない点だけで、`ledState().capsLock()` や `ledState().leds` の読み方は変わりません。ESP32KeyBridge の `OutputAdapter::getLockState()` は値をコピーして返す実装になるので、この違いは吸収されます。

## 受けた項目

| 依頼項目 | 対応 |
|---|---|
| 最新 report をメンバに保持 | `EspBleHidDeviceManagerImpl::ledState` |
| callback の有無に関係なく更新 | 下記のとおり依頼より前の位置で保存 |
| 前の接続の LED を現在値として読ませない | 最後の Host の切断時と再初期化時にクリア |
| listener 化しない | listener は追加していない（DECISIONS 26 に理由を記録） |
| 命名 `ledState()` | そのまま採用 |

### callback の有無に関係なく更新する件

依頼は「dispatch 可否の判定より前に保存」でしたが、EspBle ではもう一段手前に置く必要がありました。

`dispatchPendingOutputReports()` は callback 未設定だと早期 return するため、**callback が無いと出力 queue が一切 drain されず、容量 8 で溢れ続けます**。そのため保存は queue 投入の前、かつ**溢れ判定より前**に置いています。この状態でも `ledState()` は Host に追従します。

副作用として `ledState()` は `onOutputReport()` より**最大 1 回の `update()` ぶん先行しえます**（Host が書いた瞬間に更新され、callback は次の `update()` で配送されるため）。poll 用の API としては溢れで取り逃さないほうが重要と判断し、SPEC の約束として明記しました。LED Output Report characteristic への GATT Read が書き込み直後の値を返すのとも揃います。

### 切断時の扱い

「EspBle 側の判断でよい」とあった点は、**最後の Host が切断したらクリア**にしました。前の Host の Caps Lock を次の Host の状態として返さないためです。

併せて、同じ場所で `heldState()`（NKRO 保持状態）もクリアするよう直しました。こちらは残すと実害があります——再接続した adapter が `heldState()` と比較して重複送信を抑制する使い方をするので、切断前の状態が残っていると「前回と同じだから送らない」と判断し、**Host 側に何も押されていないのに adapter は押されていると思っている** stuck key になります。

## 複数 Host の扱い（EspBle 固有）

EspBle は最大 3 接続を持てるため、2 台の Host が別々の Caps Lock 状態を持ちえます。`ledState()` は**最後に書いた Host の値**を `connectionId` 付きで返します。

新しい妥協ではなく、LED Output Report characteristic への GATT Read が既に単一の値をどの Host へも返しているためです。Host ごとに分けたくなった時点で `ledState(connectionId)` を足す余地は残しています。

## 波及の対応状況

- `src/EspBle.h` / `src/EspBle.cpp`、`keywords.txt` — 対応済み
- `docs/HID_DEVICE_SPEC.ja.md`、`docs/DECISIONS.ja.md`（23〜27） — 対応済み
- `docs/GUIDE_BLE_BASICS.{ja.,}md` — 6.5 節「LED は逆方向」に getter との使い分けを追記
- Peer テスト — `tests/peer/hid_keyboard_nkro` に入れました（提案は `hid_keyboard_device` でしたが、LED Output を既に検証している流れがそちらにあったため）。提案どおり 2 段で、「callback あり → callback と getter が一致」と「callback を外す → それでも getter が Host に追従」を検証します
- `CHANGELOG.md` — **意図的に触っていません。** EspBle は未リリース（`version=0.1.0`）で CHANGELOG が `## Unreleased - 初期リリース` のみのため、個別項目を足すと「以前からあった API に後から追加した」という誤った読みになります。1.0.0 の初期リリースの一部として出ます

## 揃えていない既存差（今回の対象外）

依頼にあった `EspBleHidKeyboardOutputReport::numLock()`（メソッド）と `EspUsbDeviceHidKeyboardOutputReport::numLock`（bool メンバ）の差は、今回そのままにしています。揃えるなら破壊的変更なので、**1.0.0 前に別途判断が必要**です。こちらから提案する用意はあります。
