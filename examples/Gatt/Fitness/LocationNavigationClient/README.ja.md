# LocationNavigationClient

> English: [README.md](README.md)

Location and Navigation Service（0x1819）のCentral / GATT Clientです。LN Feature（0x2A6A）をReadし、Location and Speed（0x2A67）のNotificationを購読して、Instantaneous SpeedとLocationの緯度・経度をデコードします。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATT Client）
- Peripheral × 1: [LocationNavigationServer](../LocationNavigationServer/) example

## 動作

- 0x1819をadvertiseする接続可能なpeerをscanして接続
- 接続時に4byteのLN FeatureをReadし、その後Location and Speedを購読
- Instantaneous Speed（bit 0）とLocation（bit 2）の両方を含む measurement のみをデコード: speed（1/100 m/s）と緯度・経度（1e-7 度）

## 主なAPI

- `ble.readCharacteristic(...)` — LN Feature を Read
- `ble.subscribe(...)` — Location and Speed の Notification を有効化
- `ble.onNotification(callback)` — speed と location をデコード

## 期待されるSerial出力

```
Speed: 5.10 m/s, location: 35.681200, 139.767100
Speed: 5.20 m/s, location: 35.681200, 139.767100
```
