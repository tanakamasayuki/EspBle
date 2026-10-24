# IndicateServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Indication variant of [NotifyServer](../NotifyServer/): the server sends a counter value every two seconds, and each delivery is acknowledged by the client at the ATT layer. The confirmation result arrives asynchronously via `onSent()`. Pair with [Gatt/IndicateClient](../IndicateClient/).

Use indications instead of notifications when the client must not miss a value (e.g. state transitions); use notifications for high-rate streams where the ACK round trip would throttle throughput.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / GATT server)
- 1 × ESP32-S3 running the [Gatt/IndicateClient](../IndicateClient/) example

## What it does

- Registers a readable + indicatable characteristic before `begin()`
- Tracks the CCCD indication subscription via `onSubscriptionChanged()` and only sends while a subscriber exists
- Sends an incrementing counter every 2 seconds and prints whether the client confirmed each delivery

## Key APIs

- `EspBleGattCharacteristicConfig::indicatable` — adds the Indicate property and CCCD
- `gattServer.indicate(characteristic, value)` — accepted synchronously; does not block the loop while waiting for the confirmation
- `gattServer.onSent(callback)` — for indications, `result.success` means the client confirmed the delivery
- `subscription.indications` — CCCD indication state per connection

## Expected Serial output

```
Indication confirmed: 1
Indication confirmed: 2
...
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [IndicateServer.ino](IndicateServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [IndicateClient](../IndicateClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

IndicateServer - indication variant of NotifyServer: each value is delivered with an ATT confirmation from the client, and the result arrives via onSent(). Pair with Gatt/IndicateClient.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `gattServer.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `gattServer.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `gattServer.onSubscriptionChanged(...)` — Track which connection subscribed before sending it updates; connecting does not automatically enable characteristic notifications.
4. `gattServer.onSent(...)` — Use the asynchronous send result to inspect completion and failures. A notify result and an indication confirmation have different delivery guarantees.
5. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
6. `advertising.addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.
7. `ble.gattServer().indicate(...)` — Queue an indication for subscribers. It has an ATT confirmation, but immediate acceptance still precedes completion and does not prove the peer application performed an action.

## Verify and adapt

Observe values on an indication subscriber. Handle queued acceptance separately from completion and avoid starting an unbounded stream of indications when the peer cannot consume them.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
