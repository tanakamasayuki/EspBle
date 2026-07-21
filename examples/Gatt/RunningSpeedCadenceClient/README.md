# RunningSpeedCadenceClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Running Speed and Cadence Service (0x1814), reads Sensor Location, subscribes to RSC Measurement notifications, and decodes speed/cadence/stride/distance.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × RSC peripheral: the [RunningSpeedCadenceServer](../RunningSpeedCadenceServer/) example or a commercial foot pod

## What it does

- Scans for and connects to a device advertising 0x1814
- Reads Sensor Location (0x2A5D)
- Subscribes to RSC Measurement (0x2A53) notifications
- Decodes instantaneous speed (1/256 m/s) and cadence, plus optional stride length (1/100 m) and total distance (1/10 m)

## Key APIs

- `ble.subscribe(connectionId, service, characteristic)` — subscribe to notifications
- Flags-driven parsing of a mixed-width little-endian measurement

## Expected Serial output

```
Sensor location: 2
Walking: 3.00 m/s, cadence 180 /min, stride 1.25 m, distance 3.0 m
```
