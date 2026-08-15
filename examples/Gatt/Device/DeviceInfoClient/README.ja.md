# DeviceInfoClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../../docs/GUIDE_BLE_BASICS.ja.md) 4章「GATT編 — データをやり取りする」

標準Device Information Service（`0x180A`）を広告するPeripheralへ接続し、Manufacturer Name（`0x2A29`）、Model Number（`0x2A24`）、Firmware Revision（`0x2A26`）、PnP ID（`0x2A50`）を順番にReadするCentralです。

## 必要なもの

- 1 × ESP32-S3（このスケッチ。Central）
- 1 × Device Information Peripheral: [DeviceInfoServer](../DeviceInfoServer/) example

## 動作

- Active scanで`0x180A`を広告する最初の機器に接続
- 接続時に4つのCharacteristicを1つずつRead、前のResultから次のReadを連鎖
- 3つのテキストCharacteristicは文字列として表示
- 7-byteのPnP IDをVendor ID Sourceと、little-endianのVendor ID / Product ID / Product Versionへdecode

## 主なAPI

- `ble.scanner().onResult(...)` / `advertisesService(...)` — Serviceを広告するpeerを選択
- `ble.readCharacteristic(...)` + `ble.onCharacteristicRead(...)` — indexで駆動する順次Read

## 期待されるSerial出力

```
Manufacturer: EspBle
Model: DeviceInfoServer
Firmware: 0.1.0
PnP ID: source=2 vendor=0x1234 product=0x5678 version=0x0100
```

## 関連するガイド

- [BLE入門ガイド §4 GATT編](../../../../docs/GUIDE_BLE_BASICS.ja.md#4-gatt編--データをやり取りする) — service・characteristic・notify・MTU
- [BLE入門ガイド §5 UUID](../../../../docs/GUIDE_BLE_BASICS.ja.md#5-uuidを理解する) — 16-bitと128-bitの関係
