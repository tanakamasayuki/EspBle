# LocationNavigationServer

> English: [README.md](README.md)

標準Location and Navigation Service（0x1819）のPeripheralです。Location and Speed（0x2A67）をuint16 flagsフィールド＋flagsで選ばれるデータフィールドでNotifyし、LN Feature（0x2A6A）はuint32としてReadできます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- Central × 1: [LocationNavigationClient](../LocationNavigationClient/) example、または任意のLN collector

## 動作

- Location and Navigation serviceを登録し、0x1819をadvertise
- LN Feature = 0x00000005（Instantaneous Speed + Location をサポート）をReadable値として公開
- 1秒ごとに、5 m/s付近で変化する速度と固定の東京の位置を12byteのLocation and Speed measurement（flags 0x0005）としてNotify

## 主なAPI

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Location and Speed
- `ble.gattServer().setValue(...)` — Readable な LN Feature を設定
- `ble.gattServer().notify(...)` — 各measurementを購読者へ送信

## メモ

- フィールドはflag bit順に並びます: Instantaneous Speed（uint16、1/100 m/s）に続いてLocationの緯度・経度（sint32、1e-7 度）。

## 期待されるSerial出力

```
The server is silent; observe values on the client.
```
