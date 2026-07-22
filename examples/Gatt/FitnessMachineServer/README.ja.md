# FitnessMachineServer

> English: [README.md](README.md)

スマートトレーナーや屋内バイクで使われる標準Fitness Machine Service（0x1826）のPeripheralです。Indoor Bike Data（0x2AD2）を16bit flags＋instantaneous speed（0.01 km/h）・cadence（0.5/min）・符号付きpower（W）で**Notify**し、Fitness Machine Feature（0x2ACC）は8byteのfeature bitmap対をReadできます。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（Peripheral）
- Central × 1: [FitnessMachineClient](../FitnessMachineClient/) example、または任意のFitness Machine collector（Zwift等）

## 動作内容

- `begin()`前にFitness Machine serviceを登録し、0x1826をadvertise
- 1秒ごとに、speed 30〜40 km/h（上下）・cadence 90 rpm・power 250 W をIndoor Bike Data（flags 0x0044）としてNotify

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Indoor Bike Data
- `ble.gattServer().notify(...)` — 購読者へのNotification

## メモ

- Indoor Bike Dataのbit 0は*More Data*で、instantaneous speedはbit0が**0**のとき存在します。optionalフィールドはflag順（average speed、cadence、distance、resistance、power…）に並びます。
- 本exampleはデータ配信パスを示すものです。Fitness Machine Control Point（対話的なtarget/resistance制御）は未実装です。

## 期待されるSerial出力

Serverは何も出力しません。decode結果はClient側で確認します。
