# ContinuousGlucoseMonitoringClient

> English: [README.md](README.md)

Continuous Glucose Monitoring Service（0x181F）へ接続し、CGM FeatureをReadしてE2E-CRC検証、CGM Measurementを購読、各測定値のE2E-CRCを検証し、SFLOAT血糖値とtime offsetをデコードします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × CGM Peripheral: [ContinuousGlucoseMonitoringServer](../ContinuousGlucoseMonitoringServer/) example

## 動作

- 0x181FをAdvertiseする機器をscanして接続
- CGM Feature（0x2AA8）をReadし、feature bitを信頼する前にE2E-CRCを検証
- CGM Measurement（0x2AA7）の**Notification**を購読
- 各測定値のE2E-CRC（CRC-16/MCRF4XX）を検証し、SFLOAT血糖値とtime offsetをデコード

## 主なAPI

- `espBleCgmVerifyCrc(data, length)` — `EspBleCgmCrc.h`の末尾CGM E2E-CRCを検証
- `espBleReadMedicalSFloatLE(...)` — IEEE-11073 SFLOAT血糖値をデコード

## 期待されるSerial出力

```
CGM Feature: 0x001000 (E2E-CRC verified)
Glucose: 100 mg/dL at +10 min
Glucose: 101 mg/dL at +15 min
```
