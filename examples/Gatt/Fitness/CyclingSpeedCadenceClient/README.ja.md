# CyclingSpeedCadenceClient

> English: [README.md](README.md)

Cycling Speed and Cadence Service（0x1816）へ接続し、Sensor LocationをRead、CSC MeasurementのNotificationを購読して、wheel/crank回転数フィールドをデコードします。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × CSC Peripheral: [CyclingSpeedCadenceServer](../CyclingSpeedCadenceServer/) example または市販センサー

## 動作

- 0x1816をAdvertiseする機器をscanして接続
- Sensor Location（0x2A5D）をRead
- CSC Measurement（0x2A5B）のNotificationを購読
- flagsと存在する各フィールド（wheel回転数＋イベント時刻、crank回転数＋イベント時刻）をデコード

## 主なAPI

- `ble.subscribe(connectionId, service, characteristic)` — Notificationを購読
- flags駆動で多フィールドのlittle-endian measurementを解析

## 期待されるSerial出力

```
Sensor location: 12
Wheel: 100 revs, last event 2.000 s
Crank: 50 revs, last event 1.000 s
```
