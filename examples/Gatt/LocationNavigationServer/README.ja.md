# LocationNavigationServer

> English: [README.md](README.md)

標準Location and Navigation Service（0x1819）のPeripheral。Location and Speed（0x2A67）をuint16 flags＋flagsで選ばれるフィールド付きで**Notify**し、LN Feature（0x2A6A）はuint32としてReadできます。

## ハードウェア

- 1 × ESP32-S3（このスケッチ。Peripheral）
- 1 × Central: [LocationNavigationClient](../LocationNavigationClient/) example、または LN collector

## 動作

- `begin()`の前にLocation and Navigation Serviceを登録し、0x1819をAdvertise
- 1秒ごとに 5 m/s付近で変化する速度と固定の東京の位置（flags 0x0005: 速度＋位置）をNotify
- フィールドはflag bit順に並ぶ: Instantaneous Speed（uint16、1/100 m/s）に続いてLocationの緯度・経度（sint32、1e-7 度）

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Location and Speed
- `ble.gattServer().notify(...)` — 確認応答なしNotification

## 期待されるSerial出力

Server側は出力しません。値はClientで確認します。
