# RunningSpeedCadenceServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Running Speed and Cadence Service (0x1814) peripheral. RSC Measurement (0x2A53) is **notified** with instantaneous speed and cadence plus optional stride length and total distance; RSC Feature (0x2A54) and Sensor Location (0x2A5D) are readable.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [RunningSpeedCadenceClient](../RunningSpeedCadenceClient/) example, or any RSC collector

## What it does

- Registers the RSC service before `begin()` and advertises 0x1814
- Every second, notifies 3.0 m/s at cadence 180 with a growing total distance (flags 0x03)

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — RSC Measurement
- `ble.gattServer().notify(...)` — notification to subscribers

## Expected Serial output

The server is silent; observe the reading on the client.
