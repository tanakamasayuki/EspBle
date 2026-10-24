# HeartRateClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to a peripheral advertising the standard Heart Rate Service (0x180D), reads Body Sensor Location, and subscribes to Heart Rate Measurement notifications.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Heart Rate peripheral: the [HeartRateServer](../HeartRateServer/) example, or a commercial heart rate monitor

## What it does

- Scans for and connects to a device advertising 0x180D
- Reads Body Sensor Location (0x2A38)
- Subscribes to Heart Rate Measurement (0x2A37) notifications
- Decodes the measurement per its flags: 8/16-bit heart rate, optional Energy Expended, and multiple RR intervals, validating every variable-length boundary

## Key APIs

- `ble.readCharacteristic(connectionId, service, characteristic)` — read Body Sensor Location
- `ble.subscribe(connectionId, service, characteristic)` — subscribe to notifications
- `ble.onNotification(...)` — receive each Heart Rate Measurement

## Expected Serial output

```
Body Sensor Location: 1
Heart Rate subscription: ready
Heart Rate: 71 bpm, RR intervals: 1 (first: 1024/1024 s)
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HeartRateClient.ino](HeartRateClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [HeartRateServer](../HeartRateServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.
4. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
5. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
6. `ble.onSubscribed(...)` — Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
7. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
8. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
9. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

The flags select the heart-rate field width and optional data. Do not assume every Heart Rate Measurement is exactly one flags byte plus one rate byte.

The server values are simulated protocol examples, not measurements from a medical sensor. Replace data acquisition separately while preserving the service encoding and validity checks on the client.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
