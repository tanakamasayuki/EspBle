# CyclingPowerClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Cycling Power Service (0x1818), reads Sensor Location, subscribes to Cycling Power Measurement notifications, and decodes the 16-bit flags and signed 16-bit instantaneous power.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Cycling Power peripheral: the [CyclingPowerServer](../CyclingPowerServer/) example or a commercial power meter

## What it does

- Scans for and connects to a device advertising 0x1818
- Reads Sensor Location (0x2A5D)
- Subscribes to Cycling Power Measurement (0x2A63) notifications
- Decodes the signed 16-bit instantaneous power (watts)

## Key APIs

- `ble.subscribe(connectionId, service, characteristic)` — subscribe to notifications
- Signed 16-bit little-endian field decoding (`static_cast<int16_t>`)

## Expected Serial output

```
Sensor location: 6
Power: 210 W
Power: 220 W
```
