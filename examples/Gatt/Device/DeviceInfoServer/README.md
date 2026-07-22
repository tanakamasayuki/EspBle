# DeviceInfoServer

> 日本語版: [README.ja.md](README.ja.md)

Peripheral publishing the standard Device Information Service (`0x180A`) with Manufacturer Name (`0x2A29`), Model Number (`0x2A24`), Firmware Revision (`0x2A26`), and PnP ID (`0x2A50`).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [DeviceInfoClient](../DeviceInfoClient/) example, or any BLE central/phone

## What it does

- Registers four read-only characteristics before `begin()` and advertises `0x180A`
- Sets Manufacturer = "EspBle", Model = "DeviceInfoServer", Firmware = the library version string
- Encodes PnP ID in the standard 7-byte layout: Vendor ID Source `0x02` (USB-IF) + little-endian Vendor ID `0x1234`, Product ID `0x5678`, Product Version `0x0100`

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .readable = true })` — declare each read-only characteristic
- `ble.gattServer().setValue(...)` — set string and raw-byte values

## Notes

- PnP ID is a fixed 7-byte binary; multibyte fields are little-endian on the wire.

## Expected Serial output

```
Device Information Service is ready.
```
