# KeyboardDevice

> English: [README.md](README.md)

ボードをBLE HID keyboard（HID over GATT、Report Protocol、固定6キーロールオーバー）にします。PCやスマートフォンからPairingすると実際のキーボードとして入力でき、キー入力はSerialコマンドで発生させます。Host側からのLED Output Report（Num/Caps/Scroll Lock）も受信します。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（HID Device / Peripheral）
- HID Host × 1: PC、スマートフォン、または[KeyboardHost](../KeyboardHost/) exampleを動かす2台目のボード

## 動作内容

- `begin()`前にHID Keyboard Device profileを構成します（HID + Device Information + Battery Serviceが自動合成され、HID UUIDとkeyboard appearanceがadvertisingへ追加されます）
- Bondingつきでsecurityを有効化します — HOGPは暗号化linkを要求するため、HID Serviceのattributeには暗号化permissionが付与され、Input Reportは暗号化済みかつ購読済みの接続にのみ送信されます
- `a`でShift+Aを押し、`r`で全キーをreleaseします
- Hostが書き込んだLED状態の変化を表示します
- 切断のたびにadvertisingを再開します

## Serialコマンド

| コマンド | 動作 |
|---------|------|
| `a` | Shift+Aを押す（Input Reportを送信） |
| `r` | 全キーをrelease |

## 主なAPI

- `ble.hidKeyboardDevice().configure(config)` — `begin()`前に呼ぶ必要があります。`EspBleHidKeyboardDeviceConfig`でmanufacturer、PnP ID、country code、Report ID、初期Battery Levelを設定できます
- `EspBleHidKeyboardInputReport` — `modifiers` bitmask（例: `LeftShift`）と最大6個のHID usageを持つ`keys[]`
- `keyboard.sendInputReport(report)` / `keyboard.releaseAll()`
- `keyboard.onOutputReport(callback)` — HostからのLED状態（`numLock()`、`capsLock()`、`scrollLock()`）
- `keyboard.setBatteryLevel(0..100)` — Battery Serviceのlevel更新

## 期待されるSerial出力

```
Send 'a' to type Shift+A and 'r' to release all keys.
Keyboard LEDs: num=0 caps=1 scroll=0
```
