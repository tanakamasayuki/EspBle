# FitnessMachineClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Fitness Machine Service (0x1826), reads Fitness Machine Feature (0x2ACC), subscribes to Indoor Bike Data (0x2AD2), and decodes the flags-driven instantaneous speed, cadence, and power.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × peripheral: the [FitnessMachineServer](../FitnessMachineServer/) example

## What it does

- Scans for the Fitness Machine service UUID and connects
- Reads the 8-byte Fitness Machine Feature, then subscribes to Indoor Bike Data
- Walks the flag order to locate and decode instantaneous speed (0.01 km/h), cadence (0.5 /min → rpm), and signed power (W)

## Key APIs

- `ble.readCharacteristic(...)` / `ble.subscribe(...)`
- `ble.onNotification(callback)` — decode the Indoor Bike Data payload

## Expected Serial output

```
Fitness Machine Feature read; subscribing to Indoor Bike Data
Speed: 30.00 km/h  Cadence: 90 rpm  Power: 250 W
Speed: 31.00 km/h  Cadence: 90 rpm  Power: 250 W
...
```
