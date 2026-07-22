# UserDataServer

> English: [README.md](README.md)

標準User Data Service（0x181C）のPeripheral。Age（0x2A80）はread/writeのuint8、First Name（0x2A8A）はread/writeのutf8s、Database Change Increment（0x2A99）はread/write/**notify**のuint32です。AgeかFirst Nameが書かれるたびにincrementを増やしてNotifyします。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [UserDataClient](../UserDataClient/) example、または User Data collector

## 動作

- `begin()`の前にAge、First Name、Database Change Incrementを登録し、0x181CをAdvertise
- Clientの書き込みを`onWritten`で処理: 新しいAgeを保存、First Nameをログ、Database Change Incrementを増やしてNotify
- 初期値はAge 25、increment 0

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .readable = true, .writable = true })` — 書き込み可能Characteristic
- `ble.gattServer().onWritten(...)` — Clientの書き込みをloop contextで受信
- `ble.gattServer().notify(...)` — 更新したDatabase Change IncrementをNotify

## 期待されるSerial出力

```
First Name updated: Ada
Age updated: 42
```
