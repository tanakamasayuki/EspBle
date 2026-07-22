# SubscribeClient

> 日本語版: [README.ja.md](README.ja.md)

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

## Expected Serial output

```
Notification: 1
Notification: 2
Notification: 3
...
```
