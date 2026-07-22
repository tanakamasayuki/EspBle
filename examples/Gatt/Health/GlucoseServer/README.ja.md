# GlucoseServer

> English: [README.md](README.md)

**Record Access Control Point（RACP）**手続きを持つ標準Glucose Service（0x1808）のPeripheral。Clientが「Report Stored Records（all）」を書き込むと、Glucose Measurementを1件Notifyし、続けてRACP応答をIndicateします。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [GlucoseClient](../GlucoseClient/) example、または Glucose collector

## 動作

- Glucose Measurement（0x2A18, notify）、Glucose Feature（0x2A51, read）、RACP（0x2A52, write + indicate）を登録
- RACP「Report Stored Records」書込みで、Measurement（sequence番号、base time、SFLOAT濃度、type/location）を1件Notify
- BLE送信は同時1件のため、Measurement notifyとRACP応答indicateを`onSent`で順次実行

## 主なAPI

- `ble.gattServer().onWritten(...)` — RACP要求を受信
- `ble.gattServer().onSent(...)` — notify → indicate手続きを順次実行
- `espBleWriteMedicalSFloatLE(...)` — IEEE-11073 16-bit SFLOAT濃度

## 期待されるSerial出力

Server側は出力しません。レコードはClientで確認します。
