# IndicateClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

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

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [IndicateClient.ino](IndicateClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [IndicateServer](../IndicateServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

IndicateClient - indication counterpart of SubscribeClient: subscribes with notifications=false, so the stack acknowledges every received value and the server's onSent() reports the confirmed delivery. Pair with Gatt/IndicateServer.

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

Subscribe in indication mode and observe the received update. ATT confirmation acknowledges the protocol transfer, not completion of an arbitrary action in your application.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
