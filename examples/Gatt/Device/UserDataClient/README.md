# UserDataClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to a User Data Service (0x181C), subscribes to Database Change Increment notifications, reads Age, and writes a new First Name and Age. Each write bumps the increment, which arrives as a notification.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × User Data peripheral: the [UserDataServer](../UserDataServer/) example

## What it does

- Scans for and connects to a device advertising 0x181C
- Subscribes to Database Change Increment (0x2A99) **notifications**
- Reads Age (0x2A80), then writes First Name (0x2A8A) = "Ada" and Age = 42 **with response**
- Prints each Database Change Increment notification as the server bumps it

## Key APIs

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, true)` — write with response

## Expected Serial output

```
Age: 25
Database Change Increment: 1
Database Change Increment: 2
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [UserDataClient.ino](UserDataClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [UserDataServer](../UserDataServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

UserDataClient - connect to a User Data Service (0x181C), subscribe to Database Change Increment notifications, read Age, and write a new First Name and Age. Each write bumps the increment, which arrives as a notification.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
4. `ble.onSubscribed(...)` — Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
5. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.
6. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
7. `ble.writeCharacteristic(...)` — Queue a remote write and inspect its completion event. Write Without Response provides no ATT response proving the peer application acted on the data.
8. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
9. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
10. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Age, First Name and Database Change Increment use different encodings. Preserve the increment update when changing writable fields so subscribers can detect a database change; the example values are demonstration data.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
