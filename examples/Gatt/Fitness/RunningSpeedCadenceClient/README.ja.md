# RunningSpeedCadenceClient

> English: [README.md](README.md)

Running Speed and Cadence Service（0x1814）へ接続し、Sensor LocationをRead、RSC MeasurementのNotificationを購読して、speed/cadence/stride/distanceをデコードします。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × RSC Peripheral: [RunningSpeedCadenceServer](../RunningSpeedCadenceServer/) example または市販のフットポッド

## 動作

- 0x1814をAdvertiseする機器をscanして接続
- Sensor Location（0x2A5D）をRead
- RSC Measurement（0x2A53）のNotificationを購読
- 瞬間speed（1/256 m/s）とcadence、任意のstride length（1/100 m）・total distance（1/10 m）をデコード

## 主なAPI

- `ble.subscribe(connectionId, service, characteristic)` — Notificationを購読
- flags駆動で混在幅のlittle-endian measurementを解析

## 期待されるSerial出力

```
Sensor location: 2
Walking: 3.00 m/s, cadence 180 /min, stride 1.25 m, distance 3.0 m
```
