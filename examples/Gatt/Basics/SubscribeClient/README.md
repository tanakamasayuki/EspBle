# SubscribeClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to the [Gatt/NotifyServer](../NotifyServer/) example, subscribes to its notification characteristic, and prints every received value.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × ESP32-S3 running the [Gatt/NotifyServer](../NotifyServer/) example

## What it does

- Scans for the NotifyServer service UUID and connects
- Subscribes to notifications right after the connection completes
- Prints the subscribe completion result and each notification payload

## Key APIs

- `ble.subscribe(connectionId, serviceUuid, characteristicUuid, notifications)` — `true` subscribes to notifications, `false` to indications; writes the CCCD
- `ble.onSubscribed(callback)` — CCCD write completion (`result.success`)
- `ble.onNotification(callback)` — `EspBleGattNotification` with `connectionId`, UUIDs, the copied payload, and an indication flag
- `ble.unsubscribe(connectionId, serviceUuid, characteristicUuid)` — clears the CCCD

## Notes

- **Subscribing is a CCCD write.** `subscribe()` writes a bit to the CCCD attached to the peer's characteristic, and the completion arrives at `onSubscribed()`. It involves radio traffic, so a `true` return from `subscribe()` does not mean the subscription is active yet.
- **`onNotification()` receives both notifications and indications.** Tell them apart with `notification.indication` (see [IndicateClient](../IndicateClient/)).
- **Subscriptions are restored automatically after a reconnect.** `EspBleConfig::persistentSubscriptions` is on by default, so `onSubscribed()` fires without calling `subscribe()` again (see [AutoReconnectClient](../AutoReconnectClient/)). Turn it off to manage subscriptions yourself.

## Expected Serial output

```
Notification: 1
Notification: 2
Notification: 3
...
```
