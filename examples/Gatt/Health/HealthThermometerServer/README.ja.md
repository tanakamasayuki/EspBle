# HealthThermometerServer

> English: [README.md](README.md)

標準Health Thermometer Service（0x1809）のPeripheral。Temperature Measurement（0x2A1C）をIEEE-11073 32-bit FLOATで**Indicate**し、Temperature Type（0x2A1D）はReadできます。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [HealthThermometerClient](../HealthThermometerClient/) example、または Health Thermometer collector

## 動作

- `begin()`の前にHealth Thermometer Serviceを登録し、0x1809をAdvertise
- 2秒ごとに、緩やかに上昇する温度（36.50〜38.49℃）をIEEE-11073 FLOATでIndicate
- `indicate()`は購読者にのみ届くため、Clientが購読するまでは何もしない

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .indicatable = true })` — Temperature Measurement
- `espBleWriteMedicalFloat32LE(out, mantissa, exponent)` — IEEE-11073 32-bit FLOAT（`EspBleMedicalFloat.h`）
- `ble.gattServer().indicate(service, characteristic, data, length)` — 確認応答付きIndication

## 期待されるSerial出力

Server側は出力しません。温度はClientで確認します。
