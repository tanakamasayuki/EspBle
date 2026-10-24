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

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [SubscribeClient.ino](SubscribeClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [NotifyServer](../NotifyServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

SubscribeClient - connect to the Gatt/NotifyServer example, subscribe to notifications, and print each received value.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
4. `ble.onSubscribed(...)` — Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
5. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
6. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
7. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Run NotifyServer on the peer and wait for subscription success before expecting periodic values. If the peer uses indications instead, select the matching subscription mode.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
