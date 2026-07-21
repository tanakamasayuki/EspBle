# PulseOximeterClient

> English: [README.md](README.md)

Pulse Oximeter Service（0x1822）へ接続し、PLX FeaturesをRead、PLX Spot-Check MeasurementのIndicationを購読して、SpO2とpulse rateのSFLOATをデコードします。

## ハードウェア

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
