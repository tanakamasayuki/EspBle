# CyclingPowerServer

> English: [README.md](README.md)

標準Cycling Power Service（0x1818）のPeripheral。Cycling Power Measurement（0x2A63）を16bit flagsと符号付き16bit instantaneous power（ワット）で**Notify**し、Cycling Power Feature（0x2A65）とSensor Location（0x2A5D）はReadできます。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [CyclingPowerClient](../CyclingPowerClient/) example、または Cycling Power collector

## 動作

- `begin()`の前にCycling Power Serviceを登録し、0x1818をAdvertise
- 1秒ごとに 200〜300 W を上下するpower（16bit flags 0x0000）をNotify

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Cycling Power Measurement
- `ble.gattServer().notify(...)` — 購読者へのNotification

## 期待されるSerial出力

Server側は出力しません。powerはClientで確認します。
