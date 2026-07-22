# RunningSpeedCadenceClient

> 日本語版: [README.ja.md](README.ja.md)

Central / GATT client for the Running Speed and Cadence Service (0x1814). It reads Sensor Location (0x2A5D), subscribes to RSC Measurement (0x2A53) notifications, and decodes speed, cadence, and the optional stride length and total distance.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × peripheral: the [RunningSpeedCadenceServer](../RunningSpeedCadenceServer/) example

## What it does

- Scans for a connectable peer advertising 0x1814 and connects
- On connect, reads Sensor Location and prints it
- Subscribes to RSC Measurement and decodes instantaneous speed (1/256 m/s) and cadence, reporting walking/running from flags bit 2, then optional stride length (bit 0, 1/100 m) and total distance (bit 1, 1/10 m)

## Key APIs

- `ble.readCharacteristic(...)` — read Sensor Location
- `ble.subscribe(...)` — enable RSC Measurement notifications
- `ble.onNotification(callback)` — decode the mixed-width payload

## Expected Serial output

```
Sensor location: 2
Walking: 3.00 m/s, cadence 180 /min, stride 1.25 m, distance 3.0 m
Walking: 3.00 m/s, cadence 180 /min, stride 1.25 m, distance 6.0 m
```
