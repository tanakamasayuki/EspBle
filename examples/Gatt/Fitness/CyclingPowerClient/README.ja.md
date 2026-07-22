# CyclingPowerClient

> English: [README.md](README.md)

Cycling Power Service（0x1818）へ接続し、Sensor LocationをRead、Cycling Power MeasurementのNotificationを購読して、16bit flagsと符号付き16bit instantaneous powerをデコードします。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Cycling Power Peripheral: [CyclingPowerServer](../CyclingPowerServer/) example または市販のパワーメーター

## 動作

- 0x1818をAdvertiseする機器をscanして接続
- Sensor Location（0x2A5D）をRead
- Cycling Power Measurement（0x2A63）のNotificationを購読
- 符号付き16bit instantaneous power（ワット）をデコード

## 主なAPI

- `ble.subscribe(connectionId, service, characteristic)` — Notificationを購読
- 符号付き16bit little-endianフィールドのデコード（`static_cast<int16_t>`）

## 期待されるSerial出力

```
Sensor location: 6
Power: 210 W
Power: 220 W
```
