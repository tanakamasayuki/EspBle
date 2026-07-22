# CyclingSpeedCadenceClient

> English: [README.md](README.md)

Cycling Speed and Cadence Service（0x1816）のCentral / GATT Clientです。Sensor Location（0x2A5D）をReadし、CSC Measurement（0x2A5B）のNotificationを購読して、累積wheel/crank回転数フィールドをデコードします。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- Peripheral × 1: [CyclingSpeedCadenceServer](../CyclingSpeedCadenceServer/) example

## 動作

- 0x1816をadvertiseする接続可能なpeerをscanして接続
- 接続時にSensor LocationをReadして表示
- CSC Measurementを購読し、flagsバイトに従って wheel回転数 + wheelイベント時刻（bit 0）、crank回転数 + crankイベント時刻（bit 1）をデコード

## 主なAPI

- `ble.readCharacteristic(...)` — Sensor Location を Read
- `ble.subscribe(...)` — CSC Measurement の Notification を有効化
- `ble.onNotification(callback)` — wheel/crank payload をデコード

## メモ

- イベント時刻は表示時に1/1024秒単位から秒に変換されます。

## 期待されるSerial出力

```
Sensor location: 12
Wheel: 2 revs, last event 1.000 s
Crank: 1 revs, last event 1.000 s
Wheel: 4 revs, last event 2.000 s
Crank: 2 revs, last event 2.000 s
```
