# BloodPressureServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Blood Pressure Service（0x1810）のPeripheral。Blood Pressure Measurement（0x2A35）をsystolic/diastolic/meanのIEEE-11073 16-bit SFLOATで**Indicate**し、Blood Pressure Feature（0x2A49）はReadできます。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [BloodPressureClient](../BloodPressureClient/) example、または Blood Pressure collector

## 動作

- `begin()`の前にBlood Pressure Serviceを登録し、0x1810をAdvertise
- 3秒ごとに 120/80 mmHg（平均93）をSFLOATでIndicate
- `indicate()`は購読者にのみ届く

## 主なAPI

- `espBleWriteMedicalSFloatLE(out, mantissa, exponent)` — IEEE-11073 16-bit SFLOAT（`EspBleMedicalFloat.h`）
- `ble.gattServer().indicate(...)` — 確認応答付きIndication

## 期待されるSerial出力

Server側は出力しません。測定値はClientで確認します。

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
