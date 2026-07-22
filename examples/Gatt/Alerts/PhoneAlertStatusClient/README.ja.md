# PhoneAlertStatusClient

> English: [README.md](README.md)

Phone Alert Status Service（0x180E）へ接続し、Alert StatusをRead、Ringer Settingを購読、Ringer Control Point（**Write Without Response**）でSilent Mode設定→解除を行い、NotifyされたRinger Settingを毎回表示します。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Phone Alert Status Peripheral: [PhoneAlertStatusServer](../PhoneAlertStatusServer/) example

## 動作

- 0x180EをAdvertiseする機器をscanして接続
- Alert Status（0x2A3F）をReadしbitmaskを表示
- Ringer Setting（0x2A41）の**Notification**を購読
- 5秒ごとにRinger Control Point（0x2A40）をSet Silent Mode（1）とCancel Silent Mode（3）で切り替え、NotifyされたそれぞれのRinger Settingを表示

## 主なAPI

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, false)` — Write Without Response

## 期待されるSerial出力

```
Alert Status: 0x01
Ringer Setting: Silent
Ringer Setting: Normal
```
