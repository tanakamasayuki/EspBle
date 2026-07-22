# ContinuousGlucoseMonitoringServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Continuous Glucose Monitoring Service (0x181F) peripheral. CGM Feature (0x2AA8) is a readable value protected by an End-to-End CRC; CGM Measurement (0x2AA7) is **notified** with an SFLOAT glucose concentration, a time offset, and an appended E2E-CRC.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [ContinuousGlucoseMonitoringClient](../ContinuousGlucoseMonitoringClient/) example, or any CGM collector

## What it does

- Registers CGM Measurement and CGM Feature before `begin()` and advertises 0x181F
- Publishes a CGM Feature with the E2E-CRC-supported bit set (0x001000) plus a Type/Sample Location, protected by an appended E2E-CRC
- Every 3 seconds, notifies a CGM Measurement (Size, Flags, SFLOAT glucose near 100 mg/dL, time offset) with an appended E2E-CRC

## Key APIs

- `espBleCgmAppendCrc(buffer, length)` — append the CGM E2E-CRC (CRC-16/MCRF4XX) from `EspBleCgmCrc.h`
- `espBleWriteMedicalSFloatLE(...)` — encode the IEEE-11073 SFLOAT glucose concentration

## Expected Serial output

The server is silent; observe the glucose values on the client.
