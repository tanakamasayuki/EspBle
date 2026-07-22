# ConsumerControl

> English: [README.md](README.md)

音量や再生を操作するメディアキー、BLE HID Consumer Control device（HID over GATT / HOGP）です。HID Service `0x1812` をadvertiseし、キーはSerialコマンドで発生させます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（HID Device / Peripheral）
- HID Host × 1: PCやスマートフォン、または[KeyboardHost](../KeyboardHost/)を動かす2台目のボード（`onConsumerControl()` ハンドラでイベントを表示します）

## 動作

- `begin()` 前に `ble.hidConsumerControl().configure()` を呼びHID Serviceを構成します
- Bondingつきでsecurityを有効化します — HOGPは暗号化linkを要求します
- 切断のたびにadvertisingを再開します
- `+` で音量アップ、`-` で音量ダウン、`p` で再生/一時停止

## 主なAPI

- `ble.hidConsumerControl().configure()` — `begin()` 前にHID consumer-control serviceを構成
- `consumer.click(usage)` — consumer usageをpress/release。例: `ESP_BLE_HID_CONSUMER_CONTROL_VOLUME_UP`、`..._VOLUME_DOWN`、`..._PLAY_PAUSE`

## 期待されるSerial出力

```
Send '+', '-', or 'p'.
```
