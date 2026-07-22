# CyclingSpeedCadenceClient

> 日本語版: [README.ja.md](README.ja.md)

Central / GATT client for the Cycling Speed and Cadence Service (0x1816). It reads Sensor Location (0x2A5D), subscribes to CSC Measurement (0x2A5B) notifications, and decodes the cumulative wheel/crank revolution fields.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × peripheral: the [CyclingSpeedCadenceServer](../CyclingSpeedCadenceServer/) example

## What it does

- Scans for a connectable peer advertising 0x1816 and connects
- On connect, reads Sensor Location and prints it
- Subscribes to CSC Measurement and, per the flags byte, decodes wheel revolutions + wheel event time (bit 0) and crank revolutions + crank event time (bit 1)

## Key APIs

- `ble.readCharacteristic(...)` — read Sensor Location
- `ble.subscribe(...)` — enable CSC Measurement notifications
- `ble.onNotification(callback)` — decode the wheel/crank payload

## Notes

- Event times are converted from 1/1024-second units to seconds when printed.

## Expected Serial output

```
Sensor location: 12
Wheel: 2 revs, last event 1.000 s
Crank: 1 revs, last event 1.000 s
Wheel: 4 revs, last event 2.000 s
Crank: 2 revs, last event 2.000 s
```
