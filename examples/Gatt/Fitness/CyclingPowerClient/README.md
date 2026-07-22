# CyclingPowerClient

> 日本語版: [README.ja.md](README.ja.md)

Central / GATT client for the Cycling Power Service (0x1818). It reads Sensor Location (0x2A5D), subscribes to Cycling Power Measurement (0x2A63) notifications, and decodes the signed 16-bit instantaneous power.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × peripheral: the [CyclingPowerServer](../CyclingPowerServer/) example

## What it does

- Scans for a connectable peer advertising 0x1818 and connects
- On connect, reads Sensor Location and prints it
- Subscribes to Cycling Power Measurement and decodes the signed 16-bit instantaneous power (watts) that follows the 16-bit flags field

## Key APIs

- `ble.readCharacteristic(...)` — read Sensor Location
- `ble.subscribe(...)` — enable Cycling Power Measurement notifications
- `ble.onNotification(callback)` — decode the power field

## Notes

- The power field is interpreted with `static_cast<int16_t>`, so negative power (e.g. coasting) would decode correctly.

## Expected Serial output

```
Sensor location: 6
Power: 210 W
Power: 220 W
Power: 230 W
```
