# PulseOximeterClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

Pulse Oximeter Service（0x1822）へ接続し、PLX FeaturesをRead、PLX Spot-Check MeasurementのIndicationを購読して、SpO2とpulse rateのSFLOATをデコードします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × PLX Peripheral: [PulseOximeterServer](../PulseOximeterServer/) example または市販のパルスオキシメーター

## 動作

- 0x1822をAdvertiseする機器をscanして接続
- PLX Features（0x2A60）をRead
- PLX Spot-Check Measurement（0x2A5E）の**Indication**を購読（`subscribe(..., false)`）
- SpO2とpulse rateのSFLOATをデコードして表示

## 主なAPI

- `ble.subscribe(connectionId, service, characteristic, false)` — Indicationを購読
- `espBleReadMedicalSFloatLE(bytes)` — IEEE-11073 16-bit SFLOATをデコード（`EspBleMedicalFloat.h`）

## 期待されるSerial出力

```
SpO2: 98 %, pulse: 60 bpm
SpO2: 99 %, pulse: 60 bpm
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
