# RunningSpeedCadenceClient

> English: [README.md](README.md)

Running Speed and Cadence Service（0x1814）のCentral / GATT Clientです。Sensor Location（0x2A5D）をReadし、RSC Measurement（0x2A53）のNotificationを購読して、speed・cadenceと任意のstride length・total distanceをデコードします。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- Peripheral × 1: [RunningSpeedCadenceServer](../RunningSpeedCadenceServer/) example

## 動作

- 0x1814をadvertiseする接続可能なpeerをscanして接続
- 接続時にSensor LocationをReadして表示
- RSC Measurementを購読し、瞬間speed（1/256 m/s）とcadenceをデコード。flagsのbit 2から歩行/走行を判定し、任意のstride length（bit 0、1/100 m）とtotal distance（bit 1、1/10 m）を出力

## 主なAPI

- `ble.readCharacteristic(...)` — Sensor Location を Read
- `ble.subscribe(...)` — RSC Measurement の Notification を有効化
- `ble.onNotification(callback)` — 混在幅の payload をデコード

## 期待されるSerial出力

```
Sensor location: 2
Walking: 3.00 m/s, cadence 180 /min, stride 1.25 m, distance 3.0 m
Walking: 3.00 m/s, cadence 180 /min, stride 1.25 m, distance 6.0 m
```
