# Advertise

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 2, "GAP"

Starts connectable legacy advertising carrying a device name and a 16-bit service UUID (HID, `1812`). A minimal peripheral example; observe it with a generic BLE scanner app or with the paired [Scan](../Scan/) example on a second board.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- A BLE scanner — the [Scan](../Scan/) example on a second board, or a scanner app such as nRF Connect

## What it does

- Initializes the stack with the device name `EspBle Advertiser`
- Advertises the local name and the HID service UUID `1812`, merged into a single Complete List AD structure per UUID size (CSS Part A 1.1)
- Keeps advertising until reset; a central's connection attempt is accepted by the stack

## Key APIs

- `ble.begin(config)` — initialize the stack; `config.deviceName` sets the GAP device name
- `ble.advertising().setName(name)` — put the local name into the advertising payload
- `ble.advertising().addServiceUuid(uuid)` — advertise a service UUID (grouped by size into a single Complete List)
- `ble.advertising().setChannelMap(mask)` — restrict which advertising channels are used (a bit mask of `EspBleAdvertisingChannel37/38/39`; 0 means all three). Avoiding a channel that overlaps a busy Wi-Fi band costs longer discovery times
- `ble.advertising().start()` — start connectable legacy advertising; fails with `InvalidArgument` if the payload would exceed 31 bytes
- `ble.lastErrorName()` / `ble.lastErrorDetail()` — reason for a rejected request

## Expected Serial output

Silent on success. On failure:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```

## Related guides

- [BLE guide §2 GAP](../../../docs/GUIDE_BLE_BASICS.md#2-gap--finding-and-connecting) — advertising, scanning and connecting

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [Advertise.ino](Advertise.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [Scan](../Scan/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

Advertise - start connectable legacy advertising carrying a device name and a service UUID. Minimal peripheral example; observe it with a scanner app or the Gap/Scan example.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `advertising.addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

Advertising makes this device discoverable. A service UUID in the advertisement is a discovery hint; this example does not create a matching readable GATT characteristic just by listing it.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
