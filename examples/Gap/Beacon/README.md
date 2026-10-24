# Beacon

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 2, "GAP"

Broadcasts a non-connectable, non-scannable beacon carrying manufacturer data. Unlike [Advertise](../Advertise/) (a connectable peripheral), this is a pure broadcaster: no central can connect to or scan it, so it only transmits its advertising payload on the configured interval.

## Hardware

- 1 × ESP32-S3 running this sketch (broadcaster)
- A BLE scanner — the [Scan](../Scan/) example on a second board, or a scanner app such as nRF Connect

## What it does

- Broadcasts manufacturer data (company ID `0xFFFF` + a small payload here) as a non-connectable, non-scannable advertisement
- Transmits only, on the configured interval; a scanner sees it with `connectable = false` and `scannable = false`

## Key APIs

- `ble.advertising().setConnectable(false)` — non-connectable mode (no GATT connection possible)
- `ble.advertising().setScanResponseEnabled(false)` — non-scannable (pure broadcaster; no scan response)
- `ble.advertising().setManufacturerData(data, length)` — the broadcast payload
- `ble.advertising().setInterval(minMs, maxMs)` — advertising interval in ms (20..10240; the spec requires ≥ 100 ms for non-connectable advertising)
- `ble.advertising().start()` — start broadcasting

## Notes

- Replace the manufacturer data with your assigned company ID and an iBeacon layout as needed; the 31-byte legacy advertising budget applies.
- Leave `setConnectable` at its default (`true`) for a normal connectable peripheral.

## Expected Serial output

Silent on success. On failure:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```

## Related guides

- [BLE guide §2 GAP](../../../docs/GUIDE_BLE_BASICS.md#2-gap--finding-and-connecting) — advertising, scanning and connecting

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [Beacon.ino](Beacon.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [ScanDump](../../Info/ScanDump/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

Beacon - broadcast a non-connectable, non-scannable beacon carrying manufacturer data. setConnectable(false) makes it a pure broadcaster (no GATT connection possible); setScanResponseEnabled(false) makes it non-scannable; setInterval() controls the advertising interval. Observe it with the Gap/Scan example or any scanner app.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.

## Verify and adapt

Manufacturer Data is binary and starts with a company identifier. Keep its length and format explicit; a non-connectable beacon is observed by scanning, not by attempting GATT reads.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
