# UserDataClient

> English: [README.md](README.md)

User Data Service（0x181C）へ接続し、Database Change IncrementのNotificationを購読、AgeをRead、新しいFirst NameとAgeをWriteします。書き込むたびにincrementが増え、Notificationとして届きます。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × User Data Peripheral: [UserDataServer](../UserDataServer/) example

## 動作

- 0x181CをAdvertiseする機器をscanして接続
- Database Change Increment（0x2A99）の**Notification**を購読
- Age（0x2A80）をReadし、First Name（0x2A8A）= "Ada" と Age = 42 を**応答ありWrite**
- Serverがincrementを増やすたびに、そのNotificationを表示

## 主なAPI

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, true)` — 応答ありWrite

## 期待されるSerial出力

```
Age: 25
Database Change Increment: 1
Database Change Increment: 2
```
