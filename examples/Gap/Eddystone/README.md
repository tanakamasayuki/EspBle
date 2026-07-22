# Eddystone

> 日本語版: [README.ja.md](README.ja.md)

Broadcasts an **Eddystone-URL** beacon: a non-connectable, non-scannable advertisement whose Service Data (UUID `0xFEAA`) carries a compressed URL. The frame is built by the backend-independent `EspBleEddystone.h` codec.

## Hardware

- 1 × ESP32-S3 running this sketch (beacon)
- 1 × BLE scanner (the [Gap/Scan](../Scan/) example, or an Eddystone-aware app such as nRF Connect / Physical Web)

## What it does

- `espBleEncodeEddystoneUrl(...)` builds the URL frame (frame type `0x10`, TX power, scheme byte, domain-suffix-compressed URL)
- `addServiceUuid("FEAA")` — lists the Eddystone UUID so scanners discover it
- `setServiceData("FEAA", ...)` — sends the URL frame as Service Data
- `setConnectable(false)` + `setScanResponseEnabled(false)` — a pure, non-scannable broadcaster

## Key APIs

- `espBleEncodeEddystoneUrl(url, txPower, out, outCapacity, outLength)` — encode a URL frame body
- `espBleDecodeEddystoneUrl(serviceData, length, urlOut, urlCapacity, txPower)` / `espBleIsEddystoneUrl(...)` — decode/validate on the scanner side (from `EspBleScanResult::serviceData`)
- `EspBleAdvertising::setServiceData(uuid, data, length)` — attach a Service Data block

## Notes

- The URL scheme must be `http://`, `https://`, `http://www.`, or `https://www.`; common suffixes (`.com/`, `.org`, …) compress to one byte. A URL that does not fit the legacy advertising budget is rejected by the encoder.
- `txPower` is the calibrated RSSI at 0 m (dBm), used by receivers to estimate distance.
- This example broadcasts the URL frame; the `EspBleEddystone.h` codec also provides UID (`espBleEncodeEddystoneUid`) and TLM (`espBleEncodeEddystoneTlm`) frames. EID frames are not implemented.
