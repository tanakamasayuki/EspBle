# NotifyServer

> 日本語版: [README.ja.md](README.ja.md)

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
