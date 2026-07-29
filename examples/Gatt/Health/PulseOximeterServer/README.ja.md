# PulseOximeterServer

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Pulse Oximeter Service / PLX（0x1822）のPeripheral。PLX Spot-Check Measurement（0x2A5E）をSpO2とpulse rateのIEEE-11073 16-bit SFLOATで**Indicate**し、PLX Features（0x2A60）はReadできます。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [PulseOximeterClient](../PulseOximeterClient/) example、または PLX collector

## 動作

- `begin()`の前にPulse Oximeter Serviceを登録し、0x1822をAdvertise
- 3秒ごとに SpO2（95〜99 %）と 60 bpm の pulse rate をSFLOATでIndicate

## 主なAPI

- `espBleWriteMedicalSFloatLE(...)` — IEEE-11073 16-bit SFLOAT（`EspBleMedicalFloat.h`）
- `ble.gattServer().indicate(...)` — 確認応答付きIndication

## 期待されるSerial出力

Server側は出力しません。測定値はClientで確認します。
