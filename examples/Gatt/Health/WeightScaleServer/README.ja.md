# WeightScaleServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Weight Scale Service（0x181D）のPeripheral。Weight Measurement（0x2A9D）を0.005 kg分解能のuint16で**Indicate**し、Weight Scale Feature（0x2A9E）はuint32としてReadできます。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [WeightScaleClient](../WeightScaleClient/) example、または Weight Scale collector

## 動作

- `begin()`の前にWeight Scale Serviceを登録し、0x181DをAdvertise
- 3秒ごとに 70 kg付近の体重をIndicate（raw uint16、0.005 kg/LSB）
- `indicate()`は購読者にのみ届く

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .indicatable = true })` — Weight Measurement
- `ble.gattServer().indicate(...)` — 確認応答付きIndication

## 期待されるSerial出力

Server側は出力しません。体重はClientで確認します。

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
