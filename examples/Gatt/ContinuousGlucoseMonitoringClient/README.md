# ContinuousGlucoseMonitoringClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Continuous Glucose Monitoring Service (0x181F), reads and E2E-CRC-verifies CGM Feature, subscribes to CGM Measurement, verifies each measurement's E2E-CRC, and decodes the SFLOAT glucose concentration and time offset.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × CGM peripheral: the [ContinuousGlucoseMonitoringServer](../ContinuousGlucoseMonitoringServer/) example

## What it does

- Scans for and connects to a device advertising 0x181F
- Reads CGM Feature (0x2AA8) and verifies its E2E-CRC before trusting the feature bits
- Subscribes to CGM Measurement (0x2AA7) **notifications**
- Verifies each measurement's E2E-CRC (CRC-16/MCRF4XX) and decodes the SFLOAT glucose concentration and time offset

## Key APIs

- `espBleCgmVerifyCrc(data, length)` — verify the appended CGM E2E-CRC from `EspBleCgmCrc.h`
- `espBleReadMedicalSFloatLE(...)` — decode the IEEE-11073 SFLOAT glucose concentration

## Expected Serial output

```
CGM Feature: 0x001000 (E2E-CRC verified)
Glucose: 100 mg/dL at +10 min
Glucose: 101 mg/dL at +15 min
```
