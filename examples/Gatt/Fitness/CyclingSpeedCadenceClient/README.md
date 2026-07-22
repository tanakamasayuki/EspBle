# CyclingSpeedCadenceClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Cycling Speed and Cadence Service (0x1816), reads Sensor Location, subscribes to CSC Measurement notifications, and decodes the wheel/crank revolution fields.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × CSC peripheral: the [CyclingSpeedCadenceServer](../CyclingSpeedCadenceServer/) example or a commercial sensor

## What it does

- Scans for and connects to a device advertising 0x1816
- Reads Sensor Location (0x2A5D)
- Subscribes to CSC Measurement (0x2A5B) notifications
- Decodes the flags and each present field (wheel revolutions + event time, crank revolutions + event time)

## Key APIs

- `ble.subscribe(connectionId, service, characteristic)` — subscribe to notifications
- Flags-driven parsing of a multi-field little-endian measurement

## Expected Serial output

```
Sensor location: 12
Wheel: 100 revs, last event 2.000 s
Crank: 50 revs, last event 1.000 s
```
