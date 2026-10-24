# NotifyServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

A GATT server that notifies a counter value once per second, but only while at least one client subscribes to notifications. Pair it with the [Gatt/SubscribeClient](../SubscribeClient/) example.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / GATT server)
- 1 × ESP32-S3 running the [Gatt/SubscribeClient](../SubscribeClient/) example (or a smartphone GATT app that subscribes)

## What it does

- Registers a readable + notifiable characteristic before `begin()`
- Tracks the CCCD subscription state via `onSubscriptionChanged()` and only notifies while a subscriber exists
- Sends the incrementing counter as a string every second
- Reports asynchronous send failures via `onSent()`

## Nothing is sent without a subscriber

Notifications are the server's way of pushing values when it likes, but **the client decides whether that is allowed**. A notifiable characteristic automatically gets a **CCCD** (Client Characteristic Configuration Descriptor), and a subscription begins when the client writes to it.

The CCCD is therefore a **per-connection switch**. With three peers connected there are three independent states, and it is perfectly normal for only one of them to be subscribed. `notify()` goes to the subscribed connections only; the others receive nothing.

That is why this example checks the subscriber count before sending. Producing a value every second and throwing it away is wasted work, so `onSubscriptionChanged()` tracks the state. **The send itself does not fail** — there is simply nowhere for it to go.

Subscriptions are dropped on disconnect. Depending on the client's configuration they may be restored on reconnect ([AutoReconnectClient](../AutoReconnectClient/)); from the server's side that arrives as an ordinary subscription, a CCCD write after the new connection.

## Key APIs

- `EspBleGattCharacteristicConfig::notifiable` — adds the Notify property and CCCD
- `gattServer.onSubscriptionChanged(callback)` — `subscription.notifications` / `subscription.indications` per connection
- `gattServer.notify(characteristic, value)` — accepted synchronously, sent to all subscribed connections; payload larger than `mtu - 3` is rejected with `InvalidArgument`
- `gattServer.onSent(callback)` — asynchronous send result (`EspBleGattSendResult`)

## Expected Serial output

Nothing while idle. When a subscribed client disappears mid-send you may see:

```
Notification failed: ...
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [NotifyServer.ino](NotifyServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [SubscribeClient](../SubscribeClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

NotifyServer - a GATT server that notifies a counter value once per second, but only while at least one client subscribes. Pair with Gatt/SubscribeClient.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `gattServer.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `gattServer.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `gattServer.onSubscriptionChanged(...)` — Track which connection subscribed before sending it updates; connecting does not automatically enable characteristic notifications.
4. `gattServer.onSent(...)` — Use the asynchronous send result to inspect completion and failures. A notify result and an indication confirmation have different delivery guarantees.
5. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
6. `advertising.addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.
7. `ble.gattServer().notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.

## Verify and adapt

Test with SubscribeClient and distinguish stored values, subscriptions and actual notifications. Replacing a timer with a real sensor still requires pacing and checking send acceptance.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
