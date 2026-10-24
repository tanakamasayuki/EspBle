# IBeacon

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 2, "GAP"

Broadcasts an Apple iBeacon: a non-connectable, non-scannable advertisement whose manufacturer data carries a proximity UUID, major, minor, and measured power. The layout is built by the backend-independent `EspBleIBeacon.h` codec.

## Hardware

- 1 × ESP32-S3 running this sketch (broadcaster)
- A BLE scanner — the [Scan](../Scan/) example on a second board, or a beacon app such as nRF Connect / Locate Beacon

## What it does

- `espBleEncodeIBeacon(...)` builds the 25-byte iBeacon manufacturer data (Apple company ID `0x004C`, type `0x02`, length `0x15`, UUID, major, minor, measured power)
- Broadcasts it as a pure, non-scannable advertisement on the configured interval

## Key APIs

- `EspBleIBeaconData` — proximity `uuid[16]`, `major`, `minor`, `measuredPower`
- `espBleEncodeIBeacon(beacon, out)` — encode to `EspBleIBeaconManufacturerDataSize` (25) octets
- `espBleDecodeIBeacon(manufacturerData, length, beacon)` / `espBleIsIBeacon(...)` — decode/validate on the scanner side
- `ble.advertising().setConnectable(false)` / `setScanResponseEnabled(false)` / `setManufacturerData(...)` / `setInterval(...)`

## Notes

- `measuredPower` is the calibrated RSSI at 1 m (typically negative, e.g. -59), used by receivers to estimate distance.
- Replace the UUID / major / minor with your own values.

## Expected Serial output

Silent on success. On failure:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```

## Related guides

- [BLE guide §2 GAP](../../../docs/GUIDE_BLE_BASICS.md#2-gap--finding-and-connecting) — advertising, scanning and connecting

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [IBeacon.ino](IBeacon.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [ScanDump](../../Info/ScanDump/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

IBeacon - broadcast an Apple iBeacon. The iBeacon layout (proximity UUID + major + minor + measured power) is built by the backend-independent EspBleIBeacon.h codec and sent as manufacturer data on a non-connectable, non-scannable advertisement. Observe it with the Gap/Scan example or any beacon app.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.

## Verify and adapt

UUID, major and minor identify the demonstration beacon. Measured-power data is a calibration input, not a measured distance; test the encoded payload with ScanDump.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
