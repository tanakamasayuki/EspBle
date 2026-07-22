# PhoneAlertStatusServer

> English: [README.md](README.md)

標準Phone Alert Status Service（0x180E）のPeripheral。Alert Status（0x2A3F）とRinger Setting（0x2A41）はread/**notify**のuint8、Ringer Control Point（0x2A40）は**Write Without Response**です。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral、例えば電話のproxy）
- 1 × Central: [PhoneAlertStatusClient](../PhoneAlertStatusClient/) example、または Phone Alert Status client

## 動作

- `begin()`の前にAlert Status、Ringer Setting、Ringer Control Pointを登録し、0x180EをAdvertise
- 初期値はAlert Status 0x01（Ringer State稼働）、Ringer Setting Normal（1）
- Ringer Control Point書き込みで、Set Silent Mode（1）はRinger SettingをSilent（0）にしてNotify、Cancel Silent Mode（3）はNormal（1）へ戻す

## 主なAPI

- `ble.gattServer().onWritten(...)` — Ringer Control Pointコマンドを受信
- `ble.gattServer().notify(...)` — 更新したRinger SettingをNotify

## 期待されるSerial出力

```
Silent Mode
Normal Mode
```
