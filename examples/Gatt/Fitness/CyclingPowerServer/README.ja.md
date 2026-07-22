# CyclingPowerServer

> English: [README.md](README.md)

標準Cycling Power Service（0x1818）のPeripheralです。Cycling Power Measurement（0x2A63）を16bit flagsと符号付き16bit instantaneous power（ワット）でNotifyし、Cycling Power Feature（0x2A65）とSensor Location（0x2A5D）はReadできます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- Central × 1: [CyclingPowerClient](../CyclingPowerClient/) example、または任意のCycling Power collector

## 動作

- Cycling Power serviceを登録し、0x1818をadvertise
- Cycling Power Feature（0x0000000C）とSensor Location = Right Crank（6）をReadable値として公開
- 1秒ごとにpowerを200〜300 Wで増加（300を超えると200へ戻る）させ、16bit flags 0x0000（optionalフィールドなし）の4byte Cycling Power MeasurementをNotify

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Cycling Power Measurement
- `ble.gattServer().setValue(...)` — Readable な Feature / Sensor Location を設定
- `ble.gattServer().notify(...)` — 各measurementを購読者へ送信

## メモ

- instantaneous powerは16bit flagsフィールドの直後に置かれる、符号付き16bit little-endianのワット値です。

## 期待されるSerial出力

```
The server is silent; observe values on the client.
```
