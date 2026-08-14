# ServiceData

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 2, "GAP"

Advertises a Service Data block (AD type 0x16): a payload tagged with the service UUID it belongs to. This is the standard way for **a sensor to publish a reading without anyone connecting to it**.

How it differs from the Manufacturer Data used by [Beacon](../Beacon/):

| | Service Data | Manufacturer Data |
|---|---|---|
| Meaning | Defined by the spec of the service the UUID names | Vendor-specific; you need to know the company ID to interpret it |
| Allocation needed | A SIG-assigned UUID (or your own 128-bit UUID) | A company ID assigned by the Bluetooth SIG |
| Good for | Broadcasting values of standard services | Custom formats and vendor definitions such as iBeacon |

This example broadcasts a temperature under the Environmental Sensing Service UUID (`0x181A`). The payload uses the same wire format as the GATT characteristic (signed 16-bit, 0.01 °C, little-endian), so a receiver can reuse the decoding it would use over a connection.

## Hardware

- 1 × ESP32-S3 running this sketch (broadcaster)
- A receiver — the [Info/ScanDump](../../Info/ScanDump/) example on a second board, or a scanner app such as nRF Connect

## What it does

- Broadcasts a temperature as Service Data under the Environmental Sensing Service (`0x181A`)
- Updates the value every 5 seconds. Legacy advertising cannot rewrite a payload in place, so the sketch does `stop()` → `addServiceData()` → `start()`. Re-adding the same service UUID replaces that block rather than appending a second one, so the payload does not grow
- Runs as a non-connectable, non-scannable broadcaster

## Key APIs

- `ble.advertising().addServiceData(uuid, data, length)` — add a Service Data block; the AD type (0x16 / 0x20 / 0x21) follows the UUID size. Up to four blocks with distinct UUIDs; calling it again with the same UUID replaces that block, and passing no data removes it
- `ble.advertising().addServiceUuid(uuid)` — also list the UUID so a receiver's `advertisesService()` matches
- `ble.advertising().setConnectable(false)` / `setScanResponseEnabled(false)` — make it a pure broadcaster
- Receiving side: `scanResult.serviceData[]` / `serviceDataCount` / `hasServiceData()`, and `scanResult.serviceDataFor(uuid, data)` to look one up by UUID

## Notes

- Service Data consumes the 31-byte legacy advertising budget. A 128-bit UUID alone takes 16 bytes, so a custom UUID leaves little room for the payload.
- One advertisement may carry several Service Data blocks (up to four, on both the sending and receiving side). Look a block up with `serviceDataFor()` by UUID rather than by index, so the code does not depend on ordering.
- The receiver's `uuid` comes back in **full 128-bit form** (`0000181a-0000-1000-8000-00805f9b34fb`) even when the sender specified the 16-bit shorthand (`181A`). A plain string comparison will not match, so use `serviceDataFor()`, which compares by value.
- Advertising stops and restarts on every update, so the broadcast has a brief gap each time. This is not suitable for updates every few hundred milliseconds.
- **The receiver only sees the first value unless it enables duplicate reporting**, because a scanner reports each device once by default (`EspBleScanConfig::wantDuplicates = true`). In [Info/ScanDump](../../Info/ScanDump/) press `d` to toggle it.

## Expected Serial output

```
Broadcasting 23.50 degC
Broadcasting 23.75 degC
Broadcasting 24.00 degC
```

On the [Info/ScanDump](../../Info/ScanDump/) side:

```
d0:cf:13:58:fd:95 type=0 rssi=-13 uuid=0000181a-0000-1000-8000-00805f9b34fb servicedata[0000181a-0000-1000-8000-00805f9b34fb][2]=c409
```

## Related guides

- [BLE guide §2 GAP](../../../docs/GUIDE_BLE_BASICS.md#2-gap--finding-and-connecting) — advertising, scanning and connecting
