# WeightScaleServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Weight Scale Service (0x181D) peripheral. Weight Measurement (0x2A9D) is **indicated** as a uint16 weight at 0.005 kg resolution; Weight Scale Feature (0x2A9E) is a readable uint32.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [WeightScaleClient](../WeightScaleClient/) example, or any Weight Scale collector

## What it does

- Registers the Weight Scale service before `begin()` and advertises 0x181D
- Every 3 seconds, indicates a weight around 70 kg (raw uint16, 0.005 kg/LSB)
- `indicate()` only reaches subscribers

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .indicatable = true })` — Weight Measurement
- `ble.gattServer().indicate(...)` — acknowledged indication

## Expected Serial output

The server is silent; observe the weight on the client.
