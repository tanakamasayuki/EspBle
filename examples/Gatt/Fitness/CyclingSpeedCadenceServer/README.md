# CyclingSpeedCadenceServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Cycling Speed and Cadence Service (0x1816) peripheral. CSC Measurement (0x2A5B) is notified with cumulative wheel/crank revolutions and their last-event times; CSC Feature (0x2A5C) and Sensor Location (0x2A5D) are readable.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [CyclingSpeedCadenceClient](../CyclingSpeedCadenceClient/) example, or any CSC collector

## What it does

- Registers the CSC service and advertises 0x1816
- Publishes CSC Feature = Wheel + Crank (0x0003) and Sensor Location = Rear Hub (12) as readable values
- Every second, advances the counters (+2 wheel revs, +1 crank rev, +1.000 s event time) and notifies an 11-byte CSC Measurement with flags 0x03 (wheel + crank present)

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — CSC Measurement
- `ble.gattServer().setValue(...)` — seed the readable Feature / Sensor Location
- `ble.gattServer().notify(...)` — push each measurement to subscribers

## Notes

- Event times use 1/1024-second units; the sketch adds 1024 per notification.
- Wheel revolutions are a 32-bit field, crank revolutions a 16-bit field.

## Expected Serial output

```
The server is silent; observe values on the client.
```
