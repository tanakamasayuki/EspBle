# DeviceInfoClient

> 日本語版: [README.ja.md](README.ja.md)

Central that connects to a peripheral advertising the standard Device Information Service (`0x180A`) and sequentially reads Manufacturer Name (`0x2A29`), Model Number (`0x2A24`), Firmware Revision (`0x2A26`), and PnP ID (`0x2A50`).

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Device Information peripheral: the [DeviceInfoServer](../DeviceInfoServer/) example

## What it does

- Active-scans and connects to the first advertiser offering `0x180A`
- On connect, reads the four characteristics one at a time, chaining each read from the previous result
- Prints the three text characteristics as strings
- Decodes the 7-byte PnP ID into Vendor ID Source, and little-endian Vendor ID, Product ID, and Product Version

## Key APIs

- `ble.scanner().onResult(...)` / `advertisesService(...)` — pick a peer advertising the service
- `ble.readCharacteristic(...)` + `ble.onCharacteristicRead(...)` — sequential reads driven by an index

## Expected Serial output

```
Manufacturer: EspBle
Model: DeviceInfoServer
Firmware: 0.1.0
PnP ID: source=2 vendor=0x1234 product=0x5678 version=0x0100
```
