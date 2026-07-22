# CyclingSpeedCadenceServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Cycling Speed and Cadence Service (0x1816) peripheral. CSC Measurement (0x2A5B) is **notified** with cumulative wheel/crank revolutions and event times; CSC Feature (0x2A5C) and Sensor Location (0x2A5D) are readable.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [CyclingSpeedCadenceClient](../CyclingSpeedCadenceClient/) example, or any CSC collector

## What it does

- Registers the CSC service before `begin()` and advertises 0x1816
- Every second, advances wheel/crank counters and notifies a Measurement (flags 0x03)
- `notify()` only reaches subscribers

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — CSC Measurement
- `ble.gattServer().notify(...)` — notification to subscribers

## Expected Serial output

The server is silent; observe the counters on the client.
