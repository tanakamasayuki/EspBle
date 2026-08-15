# GlucoseClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

Glucose Service（0x1808）へ接続し、Glucose MeasurementのNotificationとRecord Access Control Point（RACP）のIndicationを購読し、「Report Stored Records（all）」を書き込んで、デコードしたレコードとRACP応答を表示します。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Glucose Peripheral: [GlucoseServer](../GlucoseServer/) example または市販の血糖値計

## 動作

- 0x1808をAdvertiseする機器をscanして接続
- Glucose Measurement（0x2A18）のNotificationとRACP（0x2A52）のIndicationを購読
- RACP「Report Stored Records（all）」要求（opcode 1、operator 1）を書込み
- Notifyされた各Measurement（sequence番号＋SFLOAT濃度）とRACP応答をデコード

## 主なAPI

- `ble.subscribe(..., true)` / `ble.subscribe(..., false)` — MeasurementはNotification、RACPはIndication
- `ble.writeCharacteristic(..., true)` — RACP要求の確認応答付きWrite

## 期待されるSerial出力

```
Glucose record #1: 99
RACP response: request=1 status=1
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
