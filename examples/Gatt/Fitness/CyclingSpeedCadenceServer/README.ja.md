# CyclingSpeedCadenceServer

> English: [README.md](README.md)

標準Cycling Speed and Cadence Service（0x1816）のPeripheralです。CSC Measurement（0x2A5B）を累積wheel/crank回転数と直近イベント時刻でNotifyし、CSC Feature（0x2A5C）とSensor Location（0x2A5D）はReadできます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- Central × 1: [CyclingSpeedCadenceClient](../CyclingSpeedCadenceClient/) example、または任意のCSC collector

## 動作

- CSC serviceを登録し、0x1816をadvertise
- CSC Feature = Wheel + Crank（0x0003）とSensor Location = Rear Hub（12）をReadable値として公開
- 1秒ごとにカウンタを進め（wheel +2回転、crank +1回転、イベント時刻 +1.000 s）、flags 0x03（wheel + crank あり）の11byte CSC MeasurementをNotify

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — CSC Measurement
- `ble.gattServer().setValue(...)` — Readable な Feature / Sensor Location を設定
- `ble.gattServer().notify(...)` — 各measurementを購読者へ送信

## メモ

- イベント時刻は1/1024秒単位で、Notify 1回ごとに1024を加算します。
- wheel回転数は32bit、crank回転数は16bitのフィールドです。

## 期待されるSerial出力

```
The server is silent; observe values on the client.
```
