# FitnessMachineClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to a Fitness Machine Service (0x1826), reads Fitness Machine Feature (0x2ACC), subscribes to Indoor Bike Data (0x2AD2), and decodes the flags-driven instantaneous speed, cadence, and power.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × peripheral: the [FitnessMachineServer](../FitnessMachineServer/) example

## What it does

- Scans for the Fitness Machine service UUID and connects
- Reads the 8-byte Fitness Machine Feature, then subscribes to Indoor Bike Data
- Walks the flag order to locate and decode instantaneous speed (0.01 km/h), cadence (0.5 /min → rpm), and signed power (W)

## Key APIs

- `ble.readCharacteristic(...)` / `ble.subscribe(...)`
- `ble.onNotification(callback)` — decode the Indoor Bike Data payload

## Expected Serial output

```
Fitness Machine Feature read; subscribing to Indoor Bike Data
Speed: 30.00 km/h  Cadence: 90 rpm  Power: 250 W
Speed: 31.00 km/h  Cadence: 90 rpm  Power: 250 W
...
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [FitnessMachineClient.ino](FitnessMachineClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [FitnessMachineServer](../FitnessMachineServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

FitnessMachineClient - connect to a Fitness Machine Service (0x1826), read Fitness Machine Feature, subscribe to Indoor Bike Data (0x2AD2), and decode the flags-driven instantaneous speed / cadence / power.

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

Indoor Bike Data is flags-driven: speed, cadence and power have different scales and optional positions. This example exchanges sample bike data; it does not implement a complete exercise-machine control system.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
