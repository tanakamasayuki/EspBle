# GlucoseServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Glucose Service (0x1808) peripheral with the **Record Access Control Point (RACP)** procedure. When a client writes "Report Stored Records (all)", the server notifies one Glucose Measurement and then indicates the RACP response.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [GlucoseClient](../GlucoseClient/) example, or any Glucose collector

## What it does

- Registers Glucose Measurement (0x2A18, notify), Glucose Feature (0x2A51, read), and RACP (0x2A52, write + indicate)
- On a RACP "Report Stored Records" write, notifies one measurement (sequence number, base time, SFLOAT concentration, type/location)
- Sequences the measurement notify and the RACP response indicate from `onSent` (BLE sends are single-in-flight)

## Key APIs

- `ble.gattServer().onWritten(...)` — receive the RACP request
- `ble.gattServer().onSent(...)` — sequence the notify → indicate procedure
- `espBleWriteMedicalSFloatLE(...)` — IEEE-11073 16-bit SFLOAT concentration

## Expected Serial output

The server is silent; observe the records on the client.
