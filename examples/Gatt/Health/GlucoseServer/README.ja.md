# GlucoseServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

**Record Access Control Point（RACP）**手続きを持つ標準Glucose Service（0x1808）のPeripheral。Clientが「Report Stored Records（all）」を書き込むと、Glucose Measurementを1件Notifyし、続けてRACP応答をIndicateします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [GlucoseClient](../GlucoseClient/) example、または Glucose collector

## 動作

- Glucose Measurement（0x2A18, notify）、Glucose Feature（0x2A51, read）、RACP（0x2A52, write + indicate）を登録
- RACP「Report Stored Records」書込みで、Measurement（sequence番号、base time、SFLOAT濃度、type/location）を1件Notify
- 送信はqueueされるが、Measurement配送完了を待ってRACP応答をIndicateするため`onSent`で順次実行する

## 主なAPI

- `ble.gattServer().onWritten(...)` — RACP要求を受信
- `ble.gattServer().onSent(...)` — notify → indicate手続きを順次実行
- `espBleWriteMedicalSFloatLE(...)` — IEEE-11073 16-bit SFLOAT濃度

## 期待されるSerial出力

Server側は出力しません。レコードはClientで確認します。

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
