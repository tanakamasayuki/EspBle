# DeviceInfoClient

> English: [README.md](README.md)

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
