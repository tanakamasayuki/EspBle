# CyclingPowerServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Cycling Power Service (0x1818) peripheral. Cycling Power Measurement (0x2A63) is **notified** with 16-bit flags and a signed 16-bit instantaneous power (watts); Cycling Power Feature (0x2A65) and Sensor Location (0x2A5D) are readable.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [CyclingPowerClient](../CyclingPowerClient/) example, or any Cycling Power collector

## What it does

- Registers the Cycling Power service before `begin()` and advertises 0x1818
- Every second, notifies a power ramping 200–300 W (16-bit flags 0x0000)

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Cycling Power Measurement
- `ble.gattServer().notify(...)` — notification to subscribers

## Expected Serial output

The server is silent; observe the power on the client.
