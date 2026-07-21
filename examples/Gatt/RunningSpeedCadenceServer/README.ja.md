# RunningSpeedCadenceServer

> English: [README.md](README.md)

標準Running Speed and Cadence Service（0x1814）のPeripheral。RSC Measurement（0x2A53）を瞬間speed・cadenceと任意のstride length・total distanceで**Notify**し、RSC Feature（0x2A54）とSensor Location（0x2A5D）はReadできます。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [RunningSpeedCadenceClient](../RunningSpeedCadenceClient/) example、または RSC collector

## 動作

- `begin()`の前にRSC Serviceを登録し、0x1814をAdvertise
- 1秒ごとに 3.0 m/s・cadence 180・増加するtotal distance（flags 0x03）をNotify

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — RSC Measurement
- `ble.gattServer().notify(...)` — 購読者へのNotification

## 期待されるSerial出力

Server側は出力しません。測定値はClientで確認します。
