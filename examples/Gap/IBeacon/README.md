# IBeacon

> 日本語版: [README.ja.md](README.ja.md)

Broadcasts an **Apple iBeacon**: a non-connectable, non-scannable advertisement whose manufacturer data carries a proximity UUID, major, minor, and measured power. The layout is built by the backend-independent `EspBleIBeacon.h` codec.

## Hardware

- 1 × ESP32-S3 running this sketch (beacon)
- 1 × BLE scanner (the [Gap/Scan](../Scan/) example, or a beacon app such as nRF Connect / Locate Beacon)

## What it does

- `espBleEncodeIBeacon(...)` builds the 25-byte iBeacon manufacturer data (Apple company ID `0x004C`, type `0x02`, length `0x15`, UUID, major, minor, measured power)
- `setConnectable(false)` + `setScanResponseEnabled(false)` — a pure, non-scannable broadcaster
- `setManufacturerData(...)` — sends the iBeacon payload
- `setInterval(100, 150)` — advertising interval in milliseconds

## Key APIs

- `EspBleIBeaconData` — proximity `uuid[16]`, `major`, `minor`, `measuredPower`
- `espBleEncodeIBeacon(beacon, out)` — encode to `EspBleIBeaconManufacturerDataSize` (25) octets
- `espBleDecodeIBeacon(manufacturerData, length, beacon)` / `espBleIsIBeacon(...)` — decode/validate on the scanner side

## Notes

- `measuredPower` is the calibrated RSSI at 1 m (typically negative, e.g. -59), used by receivers to estimate distance.
- Replace the UUID / major / minor with your own values.
