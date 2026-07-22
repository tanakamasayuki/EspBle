# BatteryClient

> English: [README.md](README.md)

標準Battery Service（`0x180F`）をscanし、Battery Level（`0x2A19`）をReadしてから、そのNotificationを購読するCentralです。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Battery Peripheral: [BatteryServer](../BatteryServer/) example、または標準Battery Serviceを公開する任意の機器

## 動作

- Active scanで`0x180F`を広告する最初の機器に接続
- 接続時に1-byteのBattery LevelをReadして表示
- Read成功後、Battery LevelのNotificationを購読
- 以降のレベル変更をNotificationとして受信・表示

## 主なAPI

- `ble.scanner().onResult(...)` / `advertisesService(...)` — Serviceを広告するpeerを選択
- `ble.readCharacteristic(...)` + `ble.onCharacteristicRead(...)` — 現在のレベルをRead
- `ble.subscribe(...)` + `ble.onSubscribed(...)` — Notificationを有効化
- `ble.onNotification(...)` — レベル変更を受信

## 期待されるSerial出力

```
Battery: 75%
Battery subscription: ready
Battery changed: 76%
```
