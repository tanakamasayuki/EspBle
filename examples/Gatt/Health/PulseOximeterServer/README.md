# PulseOximeterServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Pulse Oximeter Service / PLX (0x1822) peripheral. PLX Spot-Check Measurement (0x2A5E) is **indicated** with SpO2 and pulse rate as IEEE-11073 16-bit SFLOATs; PLX Features (0x2A60) is readable.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [PulseOximeterClient](../PulseOximeterClient/) example, or any PLX collector

## What it does

- Registers the Pulse Oximeter service before `begin()` and advertises 0x1822
- Every 3 seconds, indicates SpO2 (95–99 %) and a 60 bpm pulse rate as SFLOATs

## Key APIs

- `espBleWriteMedicalSFloatLE(...)` — IEEE-11073 16-bit SFLOAT (from `EspBleMedicalFloat.h`)
- `ble.gattServer().indicate(...)` — acknowledged indication

## Expected Serial output

The server is silent; observe the reading on the client.
