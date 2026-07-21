# WeightScaleClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Weight Scale Service (0x181D), reads Weight Scale Feature, subscribes to Weight Measurement indications, and decodes the uint16 weight (0.005 kg resolution).

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Weight Scale peripheral: the [WeightScaleServer](../WeightScaleServer/) example or a commercial scale

## What it does

- Scans for and connects to a device advertising 0x181D
- Reads Weight Scale Feature (0x2A9E)
- Subscribes to Weight Measurement (0x2A9D) **indications** (`subscribe(..., false)`)
- Decodes the flags + uint16 weight (SI 0.005 kg/LSB, Imperial 0.01 lb/LSB) and prints it

## Key APIs

- `ble.subscribe(connectionId, service, characteristic, false)` — subscribe to indications

## Expected Serial output

```
Weight: 70.000 kg
Weight: 70.100 kg
```
