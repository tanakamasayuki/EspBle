# GlucoseClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Glucose Service (0x1808), subscribes to Glucose Measurement notifications and Record Access Control Point (RACP) indications, then writes "Report Stored Records (all)" and prints the decoded records and the RACP response.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Glucose peripheral: the [GlucoseServer](../GlucoseServer/) example or a commercial glucose meter

## What it does

- Scans for and connects to a device advertising 0x1808
- Subscribes to Glucose Measurement (0x2A18) notifications and RACP (0x2A52) indications
- Writes the RACP "Report Stored Records (all)" request (opcode 1, operator 1)
- Decodes each notified measurement (sequence number + SFLOAT concentration) and the RACP response

## Key APIs

- `ble.subscribe(..., true)` / `ble.subscribe(..., false)` — notifications for the measurement, indications for the RACP
- `ble.writeCharacteristic(..., true)` — reliable write of the RACP request

## Expected Serial output

```
Glucose record #1: 99
RACP response: request=1 status=1
```
