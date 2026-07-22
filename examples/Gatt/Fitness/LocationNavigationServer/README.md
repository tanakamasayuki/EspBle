# LocationNavigationServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Location and Navigation Service (0x1819) peripheral. Location and Speed (0x2A67) is **notified** with uint16 flags plus flags-selected fields; LN Feature (0x2A6A) is a readable uint32.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [LocationNavigationClient](../LocationNavigationClient/) example, or any LN collector

## What it does

- Registers the Location and Navigation service before `begin()` and advertises 0x1819
- Every second, notifies a speed drifting near 5 m/s plus a fixed Tokyo location (flags 0x0005: Instantaneous Speed + Location)
- Fields appear in flag-bit order: Instantaneous Speed (uint16, 1/100 m/s), then Location latitude/longitude (sint32, 1e-7 deg)

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Location and Speed
- `ble.gattServer().notify(...)` — unacknowledged notification

## Expected Serial output

The server is silent; observe the values on the client.
