# BondManagementClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Bond Management Service (0x181E), reads the Bond Management Feature bit field, and writes the Bond Management Control Point op code "Delete bond of requesting device (LE)" (0x03) with response.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Bond Management peripheral: the [BondManagementServer](../BondManagementServer/) example

## What it does

- Scans for and connects to a device advertising 0x181E
- Reads the Bond Management Feature (0x2AA5) and prints the supported-operations bit field
- Writes the Bond Management Control Point (0x2AA4) op code 0x03 (Delete bond of requesting device, LE) **with response**

## Key APIs

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, true)` — write with response

## Expected Serial output

```
Bond Management Feature: 0x000011
Delete-bond op code sent
```
