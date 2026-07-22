# HeartRateServer

> English: [README.md](README.md)

標準Heart Rate Service（0x180D）のPeripheral。Heart Rate Measurement（0x2A37）は**Notify専用**で8-bit心拍数と1つのRR-Intervalを含み、Body Sensor Location（0x2A38）はReadできます（値1 = Chest）。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [HeartRateClient](../HeartRateClient/) example、または Heart Rate collector

## 動作

- `begin()`の前にHeart Rate Measurement（notify）とBody Sensor Location（read）を登録し、0x180DをAdvertise
- Serialから`+` / `-`を送ると心拍数を増減（1〜250 bpm）してsubscriberにNotify
- 可変長のMeasurementを送出: flags 0x10（RR-Intervalあり）、8-bit心拍数、RR-Interval 1つ

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Heart Rate Measurement
- `ble.gattServer().setValue(...)` — Measurement / Body Sensor Locationの値を更新
- `ble.gattServer().notify(service, characteristic, data, length)` — subscriberへNotify

## 期待されるSerial出力

```
Send '+' or '-' to change the heart rate and notify subscribers.
Heart rate: 71 bpm (notification accepted: 1)
Heart rate: 72 bpm (notification accepted: 1)
```
