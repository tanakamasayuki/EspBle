# WeightScaleServer

> English: [README.md](README.md)

標準Weight Scale Service（0x181D）のPeripheral。Weight Measurement（0x2A9D）を0.005 kg分解能のuint16で**Indicate**し、Weight Scale Feature（0x2A9E）はuint32としてReadできます。

## ハードウェア

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
