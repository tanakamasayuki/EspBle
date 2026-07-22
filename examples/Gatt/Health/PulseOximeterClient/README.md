# PulseOximeterClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Pulse Oximeter Service (0x1822), reads PLX Features, subscribes to PLX Spot-Check Measurement indications, and decodes the SpO2 and pulse rate SFLOATs.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × PLX peripheral: the [PulseOximeterServer](../PulseOximeterServer/) example or a commercial pulse oximeter

## What it does

- Scans for and connects to a device advertising 0x1822
- Reads PLX Features (0x2A60)
- Subscribes to PLX Spot-Check Measurement (0x2A5E) **indications** (`subscribe(..., false)`)
- Decodes the SpO2 and pulse rate SFLOATs and prints them

## Key APIs

- `ble.subscribe(connectionId, service, characteristic, false)` — subscribe to indications
- `espBleReadMedicalSFloatLE(bytes)` — decode an IEEE-11073 16-bit SFLOAT (from `EspBleMedicalFloat.h`)

## Expected Serial output

```
SpO2: 98 %, pulse: 60 bpm
SpO2: 99 %, pulse: 60 bpm
```
