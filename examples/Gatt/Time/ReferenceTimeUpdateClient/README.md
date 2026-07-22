# ReferenceTimeUpdateClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Reference Time Update Service (0x1806), reads the Time Update State, writes the Time Update Control Point (**Write Without Response**) to request then cancel a reference update, and re-reads the state each time.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Reference Time Update peripheral: the [ReferenceTimeUpdateServer](../ReferenceTimeUpdateServer/) example

## What it does

- Scans for and connects to a device advertising 0x1806
- Reads the initial Time Update State (0x2A17)
- Every 2 seconds: Get Reference Update (Control Point 1) → read State → Cancel Reference Update (Control Point 2) → read State
- Prints each Current State + Result pair

## Key APIs

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, false)` — Write Without Response

## Expected Serial output

```
Time Update State: current=0 result=0
Time Update State: current=1 result=0
Time Update State: current=0 result=1
```
