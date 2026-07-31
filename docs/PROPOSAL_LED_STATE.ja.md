# 提案: HID Keyboard DeviceのLED状態取得 `ledState()`

この文書は**提案であって確定仕様ではありません**。採用が決まった項目は[DECISIONS.ja.md](DECISIONS.ja.md)と[HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md)へ移し、この文書からは消します。

## 背景

姉妹ライブラリEspUsbDeviceがLED状態の同期getter `ledState()`を追加した（`const EspUsbDeviceHidKeyboardOutputReport &`を返す）。EspBleにも対称なAPIを置く。

EspBleのDevice側は現在`onOutputReport()`のコールバックしか持たない。Hostが書いたLED状態を後から問い合わせる手段がないため、利用側はコールバックを自前の変数へ写して保持することになる。ESP32KeyBridgeの`OutputAdapter::getLockState(LockState &)`は「今のLock状態を返せ」という同期クエリなので、adapterは必ずこのshadow copyを持たされる。

これは`heldState()`（NKROの保持状態）で解消したのと同じ構図で、**ライブラリが既に持っている状態を公開していないために、利用側がずれる余地のある写しを持つ**という形になっている。

状態自体は既にある。`EspBleHidDeviceManagerImpl::outputValue`はHostの書き込みごとに更新され、LED Output Report characteristicへのGATT Readにもこの値を返している（`src/EspBle.cpp:3613` / `3673`）。追加するのは公開経路だけである。

## 提案API

```cpp
class EspBleHidKeyboard {
public:
  // The LED state the host last wrote (Caps Lock and friends), for callers that
  // need to answer "what is it now?" instead of reacting to onOutputReport().
  // Zeroed before any host has written, and on disconnect / re-initialisation.
  // With several hosts connected this is the most recent write from any of
  // them, carrying the connectionId it came from.
  const EspBleHidKeyboardOutputReport &ledState() const;
};
```

`EspBleHidKeyboardOutputReport`は既存の型で、`connectionId` / `leds`と`numLock()` / `capsLock()` / `scrollLock()` / `compose()` / `kana()`を持つ。**新しい型は追加しない。**

## 設計判断

### 1. 更新箇所は`queueOutputReport()`とする

Boot Protocol Modeでは**LEDバイトの格納先が別のフィールド**になる。

| Protocol Mode | characteristic | 格納先 |
|---|---|---|
| Report | LED Output Report `0x2A4D` `{1, 2}` | `outputValue`（`src/EspBle.cpp:3673`） |
| Boot | Boot Keyboard Output Report `0x2A32` | `bootKeyboardOutput`（`src/EspBle.cpp:3733`） |

`outputValue`をそのまま返す実装にすると、**Hostが Boot Protocol Modeを選んだ時点からLEDが更新されなくなる**。読み出し時に`protocolMode`で分岐する手もあるが、両方の書き込み経路が既に`queueOutputReport(connectionHandle, leds)`を呼んでいるので、そこで`ledState_`を更新すれば分岐は要らない。`connectionId`の解決も同じ場所で済む。

送信側の`useBootKeyboard()`が担っている「今どちらのcharacteristicか」の判断を、受信側では**書き込み経路が既に合流している**ぶん持たずに済ませられる。

### 2. 複数Hostはlast-write-winsとする

EspBleは最大3接続を持てるため、2台のHostがそれぞれ別のCaps Lock状態を持ちうる。`ledState()`は**最後に書いたHostの値**を、その`connectionId`付きで返す。

新たな妥協ではなく、**現状のGATT Readが既にその意味論**である——単一の`outputValue`をどのHostに対しても返している。Device側の問い合わせAPIを「どれか1つ」の集約として持つ点は`ready()`と同じ判断で、Host側の`EspBleHidHost::ready(connectionId)`が接続を取るのと非対称になるのも同じ理由による。

接続ごとに持つ設計は、per-connectionの記憶とその破棄タイミング（切断時にどう畳むか）を新たに決める必要がある。実際のHID DeviceはHost 1台が大半なので、必要になった時点で`ledState(connectionId)`を追加する余地を残すにとどめる。

### 3. 更新はqueue時とし、コールバック配送時ではない

`queueOutputReport()`はstack taskから呼ばれ、コールバックは`ble.update()`から配送される。`ledState_`をどちらの時点で更新するかで挙動が変わる。

**queue時に更新する。** Output queue（容量8）が溢れてコールバックが落ちても、pollする利用側は最新状態を取り逃さない。`ledState()`はpollするためのAPIなので、コールバックの生存より状態の正しさを優先する。

代償として、`ledState()`はコールバックより**最大1回の`update()`ぶん先行しうる**。つまり「`onOutputReport()`が呼ばれた時点で`ledState()`を読むと、さらに新しい値が入っていることがある」。これはSPECへ明記する。GATT Readが書き込み直後の値を返すのと揃うので、Device全体としては一貫している。

### 4. `heldState()`と同じく、ライブラリ側で解釈しない

`ledState()`は生のLEDバイトをそのまま公開する。Hostが立てたビットの意味づけ（kanaをどう扱うか等）はライブラリの判断ではない。

## 破棄のタイミング

`resetBackend()`（切断・再初期化）で`ledState_`もクリアする。`outputValue`が`src/EspBle.cpp:7586`でクリアされているのと同じ扱いで、**前の接続のLED状態を次の接続へ持ち越さない**。Hostが変われば当然LED状態も変わるためで、`heldState()`が`releaseAll()`でクリアされるのと同じ考え方。

## 波及

- `src/EspBle.h`（`ledState()`宣言、`ledState_`メンバ）、`src/EspBle.cpp`（`queueOutputReport()`、`resetBackend()`）
- `keywords.txt`に`ledState`
- `docs/HID_DEVICE_SPEC.ja.md`——LED Output Reportの節に、`ledState()`の意味論（last-write-wins、Boot Protocol Mode対応、コールバックとの先行関係、切断でクリア）
- `docs/DECISIONS.ja.md`——HIDの節へ判断として記録
- `docs/GUIDE_BLE_BASICS.{ja.,}md`——6.5節「LEDは逆方向」に、コールバックとgetterの使い分けを一文
- `tests/peer/hid_keyboard_nkro`——既にLED Output（`DEVICE_OUTPUT leds=3`）を検証しているので、同じ流れへ「コールバック受信後に`ledState()`が同じ値を返す」を追加
- NKRO exampleはLEDを扱っていないため変更不要。LED表示を伴うexampleを足すかは別判断

## 実装順

1. `ledState_`メンバと`queueOutputReport()`での更新、`resetBackend()`でのクリア
2. `ledState()`公開とkeywords
3. SPEC / DECISIONS / guide
4. Peerテスト追加と実機実行
