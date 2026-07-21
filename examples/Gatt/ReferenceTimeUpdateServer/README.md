# ReferenceTimeUpdateServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Reference Time Update Service (0x1806) peripheral. Time Update Control Point (0x2A16) is **Write Without Response** (1 = Get Reference Update, 2 = Cancel Reference Update); Time Update State (0x2A17) is a readable 2-byte value (Current State + Result).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [ReferenceTimeUpdateClient](../ReferenceTimeUpdateClient/) example, or any Reference Time Update client

## What it does

- Registers the Control Point and State before `begin()` and advertises 0x1806
- Starts Idle with a Successful result (0, 0)
- On a Control Point write, Get Reference Update (1) transitions State to Update Pending (1, 0); Cancel Reference Update (2) returns it to Idle with a Canceled result (0, 1)

## Key APIs

- `ble.gattServer().onWritten(...)` — receive Control Point commands
- `ble.gattServer().setValue(...)` — update the read-only State

## Expected Serial output

```
Get Reference Update
Cancel Reference Update
```
