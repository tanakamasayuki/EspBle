# PhoneAlertStatusServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Phone Alert Status Service (0x180E) peripheral. Alert Status (0x2A3F) and Ringer Setting (0x2A41) are read/**notify** uint8 values; the Ringer Control Point (0x2A40) is **Write Without Response**.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral, e.g. a phone proxy)
- 1 × central: the [PhoneAlertStatusClient](../PhoneAlertStatusClient/) example, or any Phone Alert Status client

## What it does

- Registers Alert Status, Ringer Setting, and Ringer Control Point before `begin()` and advertises 0x180E
- Starts with Alert Status 0x01 (Ringer State active) and Ringer Setting Normal (1)
- On a Ringer Control Point write, Set Silent Mode (1) switches Ringer Setting to Silent (0) and notifies; Cancel Silent Mode (3) switches it back to Normal (1)

## Key APIs

- `ble.gattServer().onWritten(...)` — receive Ringer Control Point commands
- `ble.gattServer().notify(...)` — notify the updated Ringer Setting

## Expected Serial output

```
Silent Mode
Normal Mode
```
