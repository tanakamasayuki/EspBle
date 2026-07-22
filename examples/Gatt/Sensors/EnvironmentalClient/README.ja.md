# EnvironmentalClient

> English: [README.md](README.md)

Environmental Sensing Service（0x181A）のCentral / GATT Clientです。Temperature（0x2A6E）、Humidity（0x2A6F）、Pressure（0x2A6D）を順番にReadしてデコードし、その後Temperature Notificationを購読します。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- Peripheral × 1: [EnvironmentalServer](../EnvironmentalServer/) example

## 動作

- 0x181Aをadvertiseするpeerをactive scanして接続
- 3つのcharacteristic（temperature、humidity、pressure）を順番にReadし、little-endian値（0.01 ℃、0.01 %、0.1 Pa）をデコード
- 3つ目のRead後にTemperatureを購読し、以降の変更Notificationを表示

## 主なAPI

- `ble.readCharacteristic(...)` — `onCharacteristicRead`で連鎖するRead
- `ble.subscribe(...)` / `ble.onSubscribed(...)` — Temperature Notificationの有効化と確認
- `ble.onNotification(callback)` — 変更されたTemperature値をデコード

## メモ

- temperatureとhumidityは16bit（符号付き/符号なし）、pressureは32bit値です。各デコードは使用前に受信長を検証します。

## 期待されるSerial出力

```
Temperature raw: 2150 (0.01 C units)
Humidity raw: 4875 (0.01 % units)
Pressure raw: 1013250 (0.1 Pa units)
Temperature subscription: ready
Temperature changed raw: 2175 (0.01 C units)
```
