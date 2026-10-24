# RunningSpeedCadenceClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Central / GATT client for the Running Speed and Cadence Service (0x1814). It reads Sensor Location (0x2A5D), subscribes to RSC Measurement (0x2A53) notifications, and decodes speed, cadence, and the optional stride length and total distance.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × peripheral: the [RunningSpeedCadenceServer](../RunningSpeedCadenceServer/) example

## What it does

- Scans for a connectable peer advertising 0x1814 and connects
- On connect, reads Sensor Location and prints it
- Subscribes to RSC Measurement and decodes instantaneous speed (1/256 m/s) and cadence, reporting walking/running from flags bit 2, then optional stride length (bit 0, 1/100 m) and total distance (bit 1, 1/10 m)

## Key APIs

- `ble.readCharacteristic(...)` — read Sensor Location
- `ble.subscribe(...)` — enable RSC Measurement notifications
- `ble.onNotification(callback)` — decode the mixed-width payload

## Expected Serial output

```
Sensor location: 2
Walking: 3.00 m/s, cadence 180 /min, stride 1.25 m, distance 3.0 m
Walking: 3.00 m/s, cadence 180 /min, stride 1.25 m, distance 6.0 m
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [RunningSpeedCadenceClient.ino](RunningSpeedCadenceClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [RunningSpeedCadenceServer](../RunningSpeedCadenceServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

RunningSpeedCadenceClient - connect to a Running Speed and Cadence Service (0x1814), read Sensor Location, subscribe to RSC Measurement notifications, and decode speed/cadence/stride/distance.

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

Flags control the optional stride-length and total-distance fields. Convert speed, cadence and distance according to their individual units; changing a flag also requires changing the payload length and decoder offsets.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
