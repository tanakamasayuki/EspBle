# KeyboardHost

> English: [README.md](README.md)

HID Host（Central）としてBLE keyboardへ接続します。HID Service `0x1812` をscanし、Pairing後にHID ReportをDiscoveryしてキーイベントを表示します。単一の `hidHost()` オブジェクトがmouse / consumer control / system control / gamepad Reportも配送するため、市販のBLE keyboardでも、2台目のボードで動かす[KeyboardDevice](../KeyboardDevice/)・[KeyboardNkro](../KeyboardNkro/)・[CompositeKeyboardMouse](../CompositeKeyboardMouse/) exampleでも使えます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Host / Central）
- BLE HID機器 × 1: 市販keyboard、またはKeyboardDevice / KeyboardNkro / CompositeKeyboardMouseを動かす2台目のボード

## 動作

- HID Service `1812` をadvertiseする最初のconnectableな機器をscanして接続します
- Bondingつきでsecurityを有効化し、HID Discoveryは`onSecurityChanged`の成功**後**に開始します — 市販keyboardは暗号化前のHID属性アクセスを拒否するためです
- layoutをEN-USに設定し、Discovery結果（Report ID、Battery）、raw usage snapshot、layout変換したASCII値つきのキーpressイベントを表示します
- 同じ `hidHost()` からmouse、consumer control、system control、gamepadのイベントも表示します
- `c` でCaps Lock LEDを点灯、`0` で全LED消灯（接続中のみ）

## 主なAPI

- `ble.hidHost().discover(connectionId)` — 明示的なHID Discovery。再接続のたびに新しいConnection IDで呼び直します
- `keyboard.onDiscovered(cb)` — `success`、`reportId`、`hasBatteryLevel`、`batteryLevel`、`detail` を持つ `EspBleHidKeyboardHostDiscovery`
- `keyboard.onKeyboardState(cb)` — layout非依存の256-bit usage snapshot（`isDown()`、`wasPressed()`、`wasReleased()`、`modifiers`）
- `keyboard.setKeyboardLayout(EspBleKeyboardLayout::EnUs)` / `keyboard.onKeyboard(cb)` — usage単位のpress/releaseイベント。`ascii` は変換可能な場合のみ非0
- `keyboard.onMouse()` / `onConsumerControl()` / `onSystemControl()` / `onGamepad()` — 複合HIDの種別別イベント
- `keyboard.setKeyboardLeds(connectionId, num, caps, scroll)` — fire-and-forgetのLED書込み（Write Without Response）

## メモ

- Discovery / state / keyイベントはすべて `ble.update()` から配送されます。

## 期待されるSerial出力

```
Keyboard ready: report=1 battery=73%
Key pressed: usage=0x04 ascii=0x61
Keyboard state: modifiers=0x02 A=1 pressed=1 released=0
```
