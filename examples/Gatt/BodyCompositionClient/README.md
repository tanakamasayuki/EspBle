# BodyCompositionClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Body Composition Service (0x181B), reads Body Composition Feature, subscribes to Body Composition Measurement indications, and decodes Body Fat Percentage (0.1 %/LSB) plus the optional Weight field.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Body Composition peripheral: the [BodyCompositionServer](../BodyCompositionServer/) example or a commercial body-composition scale

## What it does

- Scans for and connects to a device advertising 0x181B
- Reads Body Composition Feature (0x2A9B)
- Subscribes to Body Composition Measurement (0x2A9C) **indications** (`subscribe(..., false)`)
- Decodes the flags + Body Fat Percentage, and the optional Weight (SI 0.005 kg/LSB, Imperial 0.01 lb/LSB) when flag bit 10 is set

## Key APIs

- `ble.subscribe(connectionId, service, characteristic, false)` — subscribe to indications

## Expected Serial output

```
Body fat: 27.5 %, weight: 70.000 kg
Body fat: 27.6 %, weight: 70.000 kg
```
