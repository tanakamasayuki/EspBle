# ContinuousGlucoseMonitoringServer

> English: [README.md](README.md)

標準Continuous Glucose Monitoring Service（0x181F）のPeripheral。CGM Feature（0x2AA8）はEnd-to-End CRCで保護されたread値、CGM Measurement（0x2AA7）はSFLOAT血糖値、time offset、末尾のE2E-CRC付きで**Notify**します。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [ContinuousGlucoseMonitoringClient](../ContinuousGlucoseMonitoringClient/) example、または CGM collector

## 動作

- `begin()`の前にCGM MeasurementとCGM Featureを登録し、0x181FをAdvertise
- E2E-CRC対応ビットをセットしたCGM Feature（0x001000）＋Type/Sample Locationを、末尾のE2E-CRCで保護して公開
- 3秒ごとに、CGM Measurement（Size、Flags、100 mg/dL付近のSFLOAT血糖値、time offset）を末尾のE2E-CRC付きでNotify

## 主なAPI

- `espBleCgmAppendCrc(buffer, length)` — `EspBleCgmCrc.h`のCGM E2E-CRC（CRC-16/MCRF4XX）を付加
- `espBleWriteMedicalSFloatLE(...)` — IEEE-11073 SFLOAT血糖値をエンコード

## 期待されるSerial出力

Server側は出力しません。血糖値はClientで確認します。
