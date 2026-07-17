# IndicateServer

> 日本語版: [README.ja.md](README.ja.md)

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
- `gattServer.indicate(serviceUuid, characteristicUuid, value)` — accepted synchronously; does not block the loop while waiting for the confirmation
- `gattServer.onSent(callback)` — for indications, `result.success` means the client confirmed the delivery
- `subscription.indications` — CCCD indication state per connection

## Expected Serial output

```
Indication confirmed: 1
Indication confirmed: 2
...
```
