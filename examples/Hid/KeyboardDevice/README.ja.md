# KeyboardDevice

> English: [README.md](README.md)

ボードをBLE HID keyboard（HID over GATT / HOGP、固定6キーロールオーバー）にし、HID Service `0x1812` をadvertiseします。PCやスマートフォンからPairingすると実際のキーボードとして入力でき、キー入力はSerialコマンドで発生させます。Host側からのLED Output Reportも受信します。HID Boot Protocolを有効化しているためBIOS等のboot hostにも対応します。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Device / Peripheral）
- HID Host × 1: PCやスマートフォン、または[KeyboardHost](../KeyboardHost/)を動かす2台目のボード

## 動作

- `begin()` 前にHID Keyboard profileを構成します（HID + Device Information + Battery Serviceが自動合成され、HID UUIDとkeyboard appearanceがadvertisingへ追加されます）
- Boot Protocolを有効化（opt-in）し、`onProtocolMode()` を意味あるものにします。boot hostには8-byteのboot reportが自動送信されます
- Bondingつきでsecurityを有効化します — HOGPは暗号化linkを要求します
- HostのLED状態（Num/Caps/Scroll Lock）を `onOutputReport()` で、現在のProtocol Modeを `onProtocolMode()` で表示します
- 切断のたびにadvertisingを再開します
- `a` でShift+Aを押し、`r` で全キーをreleaseします

## 主なAPI

- `ble.hidKeyboard()` / `keyboard.configure(config)` — `begin()` 前に構成。`EspBleHidKeyboardConfig` で `manufacturer` を設定し、`bootProtocol` を有効化
- `EspBleHidKeyboardInputReport` — `modifiers` bitmask（例: `LeftShift`）と最大6個のHID usageを持つ `keys[]`
- `keyboard.sendReport(report)` / `keyboard.releaseAll()`
- `keyboard.onOutputReport(cb)` — LED状態（`numLock()`、`capsLock()`、`scrollLock()`）
- `keyboard.onProtocolMode(cb)` — `EspBleHidKeyboard::BootProtocolMode` と比較

## メモ

- Input ReportのReport IDとreserved byteは内部で処理されます。
- Output ReportとProtocol Modeのイベントは `ble.update()` から配送されます。

## 期待されるSerial出力

```
Send 'a' to type Shift+A and 'r' to release all keys.
Protocol Mode: Report
Keyboard LEDs: num=0 caps=1 scroll=0
```
