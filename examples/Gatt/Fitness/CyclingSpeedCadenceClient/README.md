# CyclingSpeedCadenceClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Central / GATT client for the Cycling Speed and Cadence Service (0x1816). It reads Sensor Location (0x2A5D), subscribes to CSC Measurement (0x2A5B) notifications, and decodes the cumulative wheel/crank revolution fields.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × peripheral: the [CyclingSpeedCadenceServer](../CyclingSpeedCadenceServer/) example

## What it does

- Scans for a connectable peer advertising 0x1816 and connects
- On connect, reads Sensor Location and prints it
- Subscribes to CSC Measurement and, per the flags byte, decodes wheel revolutions + wheel event time (bit 0) and crank revolutions + crank event time (bit 1)

## Key APIs

- `ble.readCharacteristic(...)` — read Sensor Location
- `ble.subscribe(...)` — enable CSC Measurement notifications
- `ble.onNotification(callback)` — decode the wheel/crank payload

## Notes

- Event times are converted from 1/1024-second units to seconds when printed.

## Expected Serial output

```
Sensor location: 12
Wheel: 2 revs, last event 1.000 s
Crank: 1 revs, last event 1.000 s
Wheel: 4 revs, last event 2.000 s
Crank: 2 revs, last event 2.000 s
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [CyclingSpeedCadenceClient.ino](CyclingSpeedCadenceClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [CyclingSpeedCadenceServer](../CyclingSpeedCadenceServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

CyclingSpeedCadenceClient - connect to a Cycling Speed and Cadence Service (0x1816), read Sensor Location, subscribe to CSC Measurement notifications, and decode the wheel/crank revolution fields.

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

Wheel/crank revolutions and event times are cumulative counters. Compute speed/cadence from changes between samples, account for wraparound and use the protocol time units; the displayed counters are not already km/h or rpm.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
