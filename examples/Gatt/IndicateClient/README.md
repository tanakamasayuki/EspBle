# IndicateClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to the [Gatt/IndicateServer](../IndicateServer/) example and subscribes to indications. The only API difference from [SubscribeClient](../SubscribeClient/) is the last argument of `subscribe()`: `false` writes the indication bit (0x0002) to the CCCD instead of the notification bit. The stack acknowledges each received value automatically, which is what makes the server's `onSent()` report a confirmed delivery.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × ESP32-S3 running the [Gatt/IndicateServer](../IndicateServer/) example

## What it does

- Scans for the IndicateServer service UUID and connects
- Subscribes to indications right after the connection completes
- Prints each received value, labelled `Indication` via `notification.indication`

## Key APIs

- `ble.subscribe(connectionId, serviceUuid, characteristicUuid, false)` — subscribe to indications
- `ble.onNotification(callback)` — receives both notifications and indications; `notification.indication` tells them apart
- `ble.onSubscribed(callback)` — CCCD write completion

## Expected Serial output

```
Indication: 1
Indication: 2
...
```
