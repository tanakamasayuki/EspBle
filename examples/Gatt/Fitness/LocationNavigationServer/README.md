# LocationNavigationServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Location and Navigation Service (0x1819) peripheral. Location and Speed (0x2A67) is notified with a uint16 flags field plus the flags-selected data fields; LN Feature (0x2A6A) is a readable uint32.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [LocationNavigationClient](../LocationNavigationClient/) example, or any LN collector

## What it does

- Registers the Location and Navigation service and advertises 0x1819
- Publishes LN Feature = 0x00000005 (Instantaneous Speed + Location supported) as a readable value
- Every second, notifies a 12-byte Location and Speed measurement (flags 0x0005) with a speed drifting near 5 m/s plus a fixed Tokyo location

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Location and Speed
- `ble.gattServer().setValue(...)` — seed the readable LN Feature
- `ble.gattServer().notify(...)` — push each measurement to subscribers

## Notes

- Fields follow flag-bit order: Instantaneous Speed (uint16, 1/100 m/s), then Location latitude and longitude (sint32, 1e-7 degrees).

## Expected Serial output

```
The server is silent; observe values on the client.
```
