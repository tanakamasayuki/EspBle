# BodyCompositionClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to a Body Composition Service (0x181B), reads Body Composition Feature, subscribes to Body Composition Measurement indications, and decodes Body Fat Percentage (0.1 %/LSB) plus the optional Weight field.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Body Composition peripheral: the [BodyCompositionServer](../BodyCompositionServer/) example or a commercial body-composition scale

## What it does

- Scans for and connects to a device advertising 0x181B
- Reads Body Composition Feature (0x2A9B)
- Subscribes to Body Composition Measurement (0x2A9C) **indications** (`subscribe(..., false)`)
- Decodes the flags + Body Fat Percentage, and the optional Weight (SI 0.005 kg/LSB, Imperial 0.01 lb/LSB) when flag bit 10 is set

## Key APIs

- `ble.subscribe(connectionId, service, characteristic, false)` — subscribe to indications

## Expected Serial output

```
Body fat: 27.5 %, weight: 70.000 kg
Body fat: 27.6 %, weight: 70.000 kg
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [BodyCompositionClient.ino](BodyCompositionClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [BodyCompositionServer](../BodyCompositionServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

BodyCompositionClient - connect to a Body Composition Service (0x181B), read Body Composition Feature, subscribe to Body Composition Measurement indications, and decode Body Fat Percentage plus the optional Weight.

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

Body Fat Percentage and optional Weight occupy a flags-defined layout. Update the flags, byte count and offsets together when adding a field.

The server values are simulated protocol examples, not measurements from a medical sensor. Replace data acquisition separately while preserving the service encoding and validity checks on the client.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
