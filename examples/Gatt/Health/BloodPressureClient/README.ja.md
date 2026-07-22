# BloodPressureClient

> English: [README.md](README.md)

Blood Pressure Service（0x1810）へ接続し、Blood Pressure FeatureをRead、Blood Pressure MeasurementのIndicationを購読して、systolic/diastolic/meanのIEEE-11073 SFLOATをデコードします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Blood Pressure Peripheral: [BloodPressureServer](../BloodPressureServer/) example または市販の血圧計

## 動作

- 0x1810をAdvertiseする機器をscanして接続
- Blood Pressure Feature（0x2A49）をRead
- Blood Pressure Measurement（0x2A35）の**Indication**を購読（`subscribe(..., false)`）
- 各測定値のSFLOATをデコードして表示

## 主なAPI

- `ble.subscribe(connectionId, service, characteristic, false)` — Indicationを購読
- `espBleReadMedicalSFloatLE(bytes)` — IEEE-11073 16-bit SFLOATをデコード（`EspBleMedicalFloat.h`）

## 期待されるSerial出力

```
Blood pressure: 120/80 mmHg (mean 93)
```
