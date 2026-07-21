# Beacon

> 日本語版: [README.ja.md](README.ja.md)

Broadcasts a **non-connectable, non-scannable beacon** carrying manufacturer data. Unlike [Gap/Advertise](../Advertise/) (a connectable peripheral), this is a pure broadcaster: no central can connect to or scan it, so it only transmits its advertising payload on the configured interval.

## Hardware

- 1 × ESP32-S3 running this sketch (broadcaster)
- 1 × BLE scanner (the [Gap/Scan](../Scan/) example on a second board, or a scanner app such as nRF Connect)

## What it does

- `setConnectable(false)` — advertises in a non-connectable mode (no GATT connection possible)
- `setScanResponseEnabled(false)` — non-scannable (a pure broadcaster; no scan response)
- `setManufacturerData(...)` — the broadcast payload (company ID `0xFFFF` + a small payload here)
- `setInterval(100, 150)` — advertising interval in milliseconds (20..10240 ms; the BLE spec requires ≥ 100 ms for non-connectable advertising)

## Notes

- Replace the manufacturer data with your assigned company ID and an iBeacon / Eddystone layout as needed; the 31-byte legacy advertising budget applies.
- A scanner sees this advertisement with `connectable = false` and `scannable = false`.
- Leave `setConnectable` at its default (`true`) for a normal connectable peripheral.
