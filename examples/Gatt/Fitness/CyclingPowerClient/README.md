# CyclingPowerClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Central / GATT client for the Cycling Power Service (0x1818). It reads Sensor Location (0x2A5D), subscribes to Cycling Power Measurement (0x2A63) notifications, and decodes the signed 16-bit instantaneous power.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × peripheral: the [CyclingPowerServer](../CyclingPowerServer/) example

## What it does

- Scans for a connectable peer advertising 0x1818 and connects
- On connect, reads Sensor Location and prints it
- Subscribes to Cycling Power Measurement and decodes the signed 16-bit instantaneous power (watts) that follows the 16-bit flags field

## Key APIs

- `ble.readCharacteristic(...)` — read Sensor Location
- `ble.subscribe(...)` — enable Cycling Power Measurement notifications
- `ble.onNotification(callback)` — decode the power field

## Notes

- The power field is interpreted with `static_cast<int16_t>`, so negative power (e.g. coasting) would decode correctly.

## Expected Serial output

```
Sensor location: 6
Power: 210 W
Power: 220 W
Power: 230 W
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [CyclingPowerClient.ino](CyclingPowerClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [CyclingPowerServer](../CyclingPowerServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

CyclingPowerClient - connect to a Cycling Power Service (0x1818), read Sensor Location, subscribe to Cycling Power Measurement notifications, and decode the 16-bit flags and signed 16-bit instantaneous power.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.
4. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
5. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
6. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
7. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
8. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Instantaneous power is signed watts. Preserve the sign, flags and little-endian order when replacing the sample; casting everything to an unsigned number can turn a negative value into a very large power.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
