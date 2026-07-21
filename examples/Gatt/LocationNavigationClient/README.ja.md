# LocationNavigationClient

> English: [README.md](README.md)

Location and Navigation Service（0x1819）へ接続し、LN FeatureをRead、Location and SpeedのNotificationを購読して、Instantaneous Speed（1/100 m/s）とLocationの緯度・経度（1e-7 度）をデコードします。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Location and Navigation Peripheral: [LocationNavigationServer](../LocationNavigationServer/) example または市販のLNセンサー

## 動作

- 0x1819をAdvertiseする機器をscanして接続
- LN Feature（0x2A6A）をRead
- Location and Speed（0x2A67）の**Notification**を購読
- flags＋Instantaneous Speed＋Locationの緯度・経度をデコードして表示

## 主なAPI

- `ble.subscribe(connectionId, service, characteristic)` — Notificationを購読

## 期待されるSerial出力

```
Speed: 5.00 m/s, location: 35.681200, 139.767100
Speed: 5.10 m/s, location: 35.681200, 139.767100
```
