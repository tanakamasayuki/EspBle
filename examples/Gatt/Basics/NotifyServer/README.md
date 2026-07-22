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

## Key APIs

- `EspBleGattCharacteristicConfig::notifiable` — adds the Notify property and CCCD
- `gattServer.onSubscriptionChanged(callback)` — `subscription.notifications` / `subscription.indications` per connection
- `gattServer.notify(serviceUuid, characteristicUuid, value)` — accepted synchronously, sent to all subscribed connections; payload larger than `mtu - 3` is rejected with `InvalidArgument`
- `gattServer.onSent(callback)` — asynchronous send result (`EspBleGattSendResult`)

## Expected Serial output

Nothing while idle. When a subscribed client disappears mid-send you may see:

```
Notification failed: ...
```
