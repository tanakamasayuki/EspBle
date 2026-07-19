# KeyboardHost

> English: [README.md](README.md)

HID HostとしてBLE HID機器へ接続します。HID Serviceをscanし、Pairing後に複合ReportをDiscoveryして、keyboard / mouse / consumer control / system control / gamepadイベントを表示します。市販のBLE keyboardでも、2台目のボードで動かす[KeyboardDevice](../KeyboardDevice/)や[CompositeKeyboardMouse](../CompositeKeyboardMouse/) exampleでも使えます。keyboard LEDの書込みも実演します。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（HID Host / Central）
- BLE keyboard × 1（市販品、またはKeyboardDeviceを動かす2台目のボード）

## 動作内容

- HID Service `1812`をadvertiseする機器をscanし、最初の1台へ接続します
- Bondingつきでsecurityを有効化し、HID Discoveryは`onSecurityChanged`の成功**後**に開始します — 市販keyboardはHID attributeへ暗号化linkを要求するためです
- Discovery結果（keyboard Report ID、Battery Level）、raw usage状態の変化、layout変換したASCII値つきのキーpressイベントを表示します
- 同じ`hidHost()`からmouse、consumer control、system control、gamepadのイベントを表示します
- `onKeyboard()`変換用のlayoutをEN-USに設定します（EspUsbHost互換の19 layoutが利用可能）
- `c`でCaps Lock LEDを点灯し、`0`でLEDを消灯します

## Serialコマンド

| コマンド | 動作 |
|---------|------|
| `c` | keyboardのCaps Lock LEDを点灯 |
| `0` | keyboardの全LEDを消灯 |

## 主なAPI

- `ble.hidHost().discover(connectionId)` — 明示的なHID Discovery。再接続のたびに新しいConnection IDで呼び直します
- `keyboard.onDiscovered(callback)` — `success`、`reportId`、Battery情報、`detail`を持つ`EspBleHidKeyboardHostDiscovery`
- `keyboard.onKeyboardState(callback)` — layout非依存の256-bit usage snapshot（`isDown()`、`wasPressed()`、`wasReleased()`）
- `keyboard.setKeyboardLayout(layout)` / `keyboard.onKeyboard(callback)` — usage単位のpress/releaseイベント。`unicode`と`ascii`（ISO-8859-1）の変換値つき
- `keyboard.onMouse()` / `onConsumerControl()` / `onSystemControl()` — 複合HIDの種別別イベント
- `keyboard.onGamepad()` — Report Mapから分解した`EspBleHidFieldValue`配列つきgamepadイベント
- `keyboard.setKeyboardLeds(connectionId, num, caps, scroll)` — fire-and-forgetのLED書込み（Write Without Response）

## 期待されるSerial出力

```
Keyboard ready: report=1 battery=73%
Key pressed: usage=0x04 ascii=0x61
Keyboard state: modifiers=0x02 A=1 pressed=1 released=0
Mouse: x=5 y=-3 wheel=0 buttons=0x00
Gamepad: fields=39 changed=1
```
