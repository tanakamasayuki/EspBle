# FitnessMachineServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Fitness Machine Service (0x1826) peripheral, as used by smart trainers and indoor bikes. Indoor Bike Data (0x2AD2) is **notified** with 16-bit flags followed by instantaneous speed (0.01 km/h), cadence (0.5 /min), and signed power (W); Fitness Machine Feature (0x2ACC) is a readable 8-byte pair of feature bitmaps.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [FitnessMachineClient](../FitnessMachineClient/) example, or any Fitness Machine collector (Zwift, etc.)

## What it does

- Registers the Fitness Machine service before `begin()` and advertises 0x1826
- Every second, notifies Indoor Bike Data with speed ramping 30–40 km/h, 90 rpm cadence, and 250 W power (flags 0x0044)

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Indoor Bike Data
- `ble.gattServer().notify(...)` — notification to subscribers

## Notes

- Indoor Bike Data bit 0 is *More Data*: instantaneous speed is present when it is **0**. Optional fields follow the flag order (average speed, cadence, distance, resistance, power, …).
- This example demonstrates the data-broadcast path; the Fitness Machine Control Point (interactive target/resistance control) is not implemented.

## Expected Serial output

The server is silent; observe the decoded values on the client.
