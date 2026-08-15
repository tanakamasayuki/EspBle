# BodyCompositionServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Body Composition Service（0x181B）のPeripheral。Body Composition Measurement（0x2A9C）をuint16 flags、必須のBody Fat Percentage（0.1 %/LSB）、任意フィールド付きで**Indicate**し、Body Composition Feature（0x2A9B）はuint32としてReadできます。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [BodyCompositionClient](../BodyCompositionClient/) example、または Body Composition collector

## 動作

- `begin()`の前にBody Composition Serviceを登録し、0x181BをAdvertise
- 3秒ごとに 27.5 %付近で変化する体脂肪率と固定の70.000 kg体重（flag bit 10をセット）をIndicate
- `indicate()`は購読者にのみ届く

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .indicatable = true })` — Body Composition Measurement
- `ble.gattServer().indicate(...)` — 確認応答付きIndication

## 期待されるSerial出力

Server側は出力しません。値はClientで確認します。

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
