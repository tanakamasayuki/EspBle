# HeartRateClient

> English: [README.md](README.md)

標準Heart Rate Service（0x180D）をAdvertiseするPeripheralへ接続し、Body Sensor LocationをReadして、Heart Rate MeasurementのNotificationを購読します。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Heart Rate Peripheral: [HeartRateServer](../HeartRateServer/) example、または市販の心拍計

## 動作

- 0x180DをAdvertiseする機器をscanして接続
- Body Sensor Location（0x2A38）をRead
- Heart Rate Measurement（0x2A37）のNotificationを購読
- Measurementをflagsに従ってデコード: 8/16-bit心拍数、任意のEnergy Expended、複数のRR-Intervalを、可変長の境界を検証しながら処理

## 主なAPI

- `ble.readCharacteristic(connectionId, service, characteristic)` — Body Sensor LocationをRead
- `ble.subscribe(connectionId, service, characteristic)` — Notificationを購読
- `ble.onNotification(...)` — 各Heart Rate Measurementを受信

## 期待されるSerial出力

```
Body Sensor Location: 1
Heart Rate subscription: ready
Heart Rate: 71 bpm, RR intervals: 1 (first: 1024/1024 s)
```
