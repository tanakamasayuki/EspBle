# AlertNotificationServer

> English: [README.md](README.md)

標準Alert Notification Service（0x1811）のPeripheral。Supported New Alert Category（0x2A47）はreadableなuint16 bitmask、New Alert（0x2A46）はCategory ID＋count＋text付きの**Notification**、Alert Notification Control Point（0x2A44）はwritableです。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [AlertNotificationClient](../AlertNotificationClient/) example、または Alert Notification client

## 動作

- `begin()`の前にcategory bitmask、New Alert、Control Pointを登録し、0x1811をAdvertise
- Email（bit 1）とSMS/MMS（bit 5）カテゴリ対応（0x0022）を提示
- 「Notify New Alert Immediately」（command 2）のControl Point書き込みで、要求されたカテゴリのNew Alert（count 3、text "Bob"）をNotify

## 主なAPI

- `ble.gattServer().onWritten(...)` — Control Pointコマンドを受信
- `ble.gattServer().notify(...)` — New AlertをNotify

## 期待されるSerial出力

```
Notify New Alert for category 1
```
