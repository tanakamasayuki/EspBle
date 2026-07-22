# Beacon

> 日本語版: [README.ja.md](README.ja.md)

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
