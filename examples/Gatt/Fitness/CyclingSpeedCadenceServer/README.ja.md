# CyclingSpeedCadenceServer

> English: [README.md](README.md)

標準Cycling Speed and Cadence Service（0x1816）のPeripheral。CSC Measurement（0x2A5B）を累積wheel/crank回転数とイベント時刻で**Notify**し、CSC Feature（0x2A5C）とSensor Location（0x2A5D）はReadできます。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [CyclingSpeedCadenceClient](../CyclingSpeedCadenceClient/) example、または CSC collector

## 動作

- `begin()`の前にCSC Serviceを登録し、0x1816をAdvertise
- 1秒ごとにwheel/crankカウンタを進めてMeasurement（flags 0x03）をNotify
- `notify()`は購読者にのみ届く

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — CSC Measurement
- `ble.gattServer().notify(...)` — 購読者へのNotification

## 期待されるSerial出力

Server側は出力しません。カウンタはClientで確認します。
