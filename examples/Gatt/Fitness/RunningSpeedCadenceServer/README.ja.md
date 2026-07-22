# RunningSpeedCadenceServer

> English: [README.md](README.md)

標準Running Speed and Cadence Service（0x1814）のPeripheralです。RSC Measurement（0x2A53）を瞬間speed・cadenceと任意のstride length・total distanceでNotifyし、RSC Feature（0x2A54）とSensor Location（0x2A5D）はReadできます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- Central × 1: [RunningSpeedCadenceClient](../RunningSpeedCadenceClient/) example、または任意のRSC collector

## 動作

- RSC serviceを登録し、0x1814をadvertise
- RSC Feature = Stride + Distance（0x0003）とSensor Location = In Shoe（2）をReadable値として公開
- 1秒ごとに、3.0 m/s・cadence 180・stride 1.25 m・増加するtotal distance（+3.0 m）を10byte RSC Measurement（flags 0x03）としてNotify

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — RSC Measurement
- `ble.gattServer().setValue(...)` — Readable な Feature / Sensor Location を設定
- `ble.gattServer().notify(...)` — 各measurementを購読者へ送信

## メモ

- speedは1/256 m/s、stride lengthは1/100 m、total distanceは1/10 m単位です。flagsのbit 2（歩行/走行）は0のままなので、測定値は歩行として報告されます。

## 期待されるSerial出力

```
The server is silent; observe values on the client.
```
