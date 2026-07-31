# 返答2: LED 状態 getter — 確認事項への回答と Lock フラグの形

宛先: EspUsbDevice / ESP32KeyBridge
対象: EspUsbDevice `docs/ESPBLE_LED_STATE_REPLY_NOTES.ja.md`

この文書は提出後に削除します。確定した判断は[DECISIONS.ja.md](DECISIONS.ja.md)（HID 23〜28）と[HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md)にあります。

## 1. 前回の返答の誤りを訂正します

「USB 側で参照が成立するのは `onHidSetReport()` の実行 context がこの制約を持たないため」は誤りでした。EspUsbDevice が自前の FreeRTOS task（`espusb-device`）で `tud_task()` を回している以上、同じ task 境界があります。**値返しという結論は合っていましたが、そちらの実装を変える必要がないと書いた根拠が間違っていました。**

指摘がなければ、EspUsbDevice に「`leds` は新しいが `capsLock` は古い」という中間状態を読める競合が残っていました。

`std::atomic<uint8_t> ledsRaw_` から都度 report を組み立てる形は、EspBle の mutex 方式より素直です。EspBle は `EspBleHidKeyboardOutputReport` に `connectionId` も入り単一 byte へ畳めないため mutex のままにしますが、**保持を raw byte 1 個に絞ってフィールド間の不整合を原理的に潰す**という考え方は、下記 3 の判断に取り込みました。

## 2. 確認事項への回答: 競合しません

> 切断時の `heldState()` クリアはどの task で走り、`heldState()` の呼び出し側はロック外でその実体を読むことにならないか

**クリアは `ble.update()` を呼んだ task（＝スケッチの loop task）で走ります。** stack task ではありません。`dispatchConnectionEvents()` 内で行っており、これは `EspBle::update()` から呼ばれます。

`nkroState_` の書き手を全て洗い出した結果です。

| 書き手 | task |
|---|---|
| `enableNkro()` / `sendReport()` ×2 / `pressUsage()` / `releaseUsage()` / `releaseAll()` | 呼び出し側 = loop |
| `resetBackend()`（`end()` / 再 `begin()`） | 呼び出し側 = loop |
| 切断時クリア（`dispatchConnectionEvents()`） | `update()` = loop |

書き手は単一 task なので、`heldState()` が `const &` を返しても呼び出し側がロック外で他 task の書き込みと競合することはありません。**`applyPendingBusChange()` 相当の遅延クリアは BLE 側では不要です。**

これは偶然ではなく、EspBle の「stack callback ではユーザ callback を実行せず、値へコピーして queue へ積み、配送は必ず `update()` 契機」という基本設計（DECISIONS アーキテクチャ 3）の帰結です。切断イベントも同じ経路を通るため、自動的に loop task 側の処理になります。

一方 `ledState` は GATT access callback から直接書くため、mutex 内でクリアし、`ledState()` も mutex 内でコピーを返しています。同じライブラリ内で扱いが分かれるのはこのためです。

そちらの遅延クリア（USB task は atomic flag を立てるだけ、実クリアはスケッチ task の入口）は、`tud_task()` を自前 task で回す構成に対する妥当な解だと思います。

## 3. Lock フラグは bool メンバへ変更しました（破壊的・対応済み）

「EspBle 側の判断に合わせる」とのことでしたので、**メンバ形に揃えました。** そちらの `numLock` メンバがそのまま正解です。

決め手は、判断材料として挙げられていなかった点です。**EspBle 内部で既に不統一でした。**

| 型 | 変更前 | 変更後 |
|---|---|---|
| `EspBleHidKeyboardState`（Host） | `bool numLock;` メンバ | 変更なし |
| `EspBleHidKeyboardOutputReport`（Device） | `bool numLock() const` メソッド | `bool numLock;` メンバ |

同じ「Lock 状態」という概念が Host 側と Device 側で別の形をしていて、しかも姉妹ライブラリは Host 側と同じ形でした。`keys` / `bitmap` とまったく同じ構図です。メンバへ揃えると**ライブラリ内の不統一と 3 ライブラリ間の不統一が同時に解消します。**

「raw byte から都度計算するので状態を二重に持たない」という利点は認識していましたが、この型は**ライブラリだけが生成するイベント payload** で、確認したところ example・テスト全 7 箇所すべてが受け取り側であり、利用者が `leds` だけ設定して不整合を作る経路が実質ありません。

そのうえで、1 で学んだ「フィールド間の不整合を原理的に潰す」考え方を取り入れ、**`setLeds(uint8_t)` を唯一の設定経路**にしました。

```cpp
struct EspBleHidKeyboardOutputReport
{
  EspBleConnectionId connectionId = 0;
  uint8_t leds = 0;
  bool numLock = false;
  bool capsLock = false;
  bool scrollLock = false;
  bool compose = false;
  bool kana = false;

  // どのビットが何を意味するかを決める唯一の場所。
  void setLeds(uint8_t value);
};
```

ライブラリ側は `setLeds()` しか呼ばないので、`leds` とフラグが食い違う状態を作れません。そちらの `makeKeyboardOutputReport()` と同じ役割です。

利用側の変更は `report.capsLock()` → `report.capsLock` の機械的な置換で、EspBle 内では example 1 箇所と peer テスト 6 箇所でした。

## 4. 「callback を外しても getter が追従」のテストを追加しました

依頼にあった 2 段構成の後半が未検証でしたので入れました。EspBle では queue の事情があるため、そちらより強い条件で確認しています。

- callback を外す（`onOutputReport(nullptr)`）
- Host が **10 回** LED を書く（Device の出力 queue は容量 8 なので確実に溢れる）
- `ledState()` が最後に書かれた値を返すことを確認

callback 未設定だと `dispatchPendingOutputReports()` が早期 return して queue が一切 drain されないため、この条件が「保存位置が queue 投入より前」であることの実証になります。

Peer テストは 87 件に増え、この 5 件を含む `hid_keyboard_nkro` スイートは実機で通っています。

## 5. 現時点で両者の差として残るもの

| 項目 | EspBle | EspUsbDevice | 揃えるか |
|---|---|---|---|
| `ledState()` 戻り値 | 値 | 値 | 揃った |
| Lock フラグ | メンバ | メンバ | 揃った |
| 保持の実装 | mutex + struct | `atomic<uint8_t>` | **揃えない。** `connectionId` を持つ EspBle は単一 byte へ畳めないため |
| `ledState()` の callback に対する先行 | 起きうる（最大 1 回の `update()`） | 起きない | **揃えない。** queue 配送モデルの差で、SPEC に書き分け済み |
| 切断時クリアの実装 | 直接クリア（loop task） | 遅延クリア（atomic flag） | **揃えない。** 上記 2 のとおり task 構成が違うため |

いずれも「同じ問題に対して各 backend で正しい解が違う」ケースで、公開 API の形は揃っているため利用側からは差が見えません。

現時点で未決の項目はありません。
