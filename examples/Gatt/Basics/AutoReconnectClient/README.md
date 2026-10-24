# AutoReconnectClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to the [Gatt/NotifyServer](../NotifyServer/) example and subscribes once. With auto-reconnect enabled and persistent subscriptions (on by default), the link and the subscription are restored automatically after an unexpected disconnect — notifications resume with no extra code.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × ESP32-S3 running the [Gatt/NotifyServer](../NotifyServer/) example

## What it does

- Scans for the NotifyServer service UUID and connects
- Subscribes once and calls `setAutoReconnect(true)`
- On an unexpected drop, the library reconnects to the same peer address and restores the subscription automatically (persistent subscriptions), so notifications resume

## Key APIs

- `ble.setAutoReconnect(true)` — remember every central peer and reconnect to it automatically on an unexpected drop; a `disconnect()` is intentional and is not reconnected
- `EspBleConfig::persistentSubscriptions` — on by default; a successful `subscribe()` is remembered per peer and restored on reconnect
- `ble.onConnected` / `ble.onDisconnected` / `ble.onSubscribed` / `ble.onNotification`

## Expected Serial output

```
Connected to ...
Subscription active (restored automatically after a reconnect).
Notification: 1
Notification: 2
Disconnected - auto-reconnect will restore the link.
Connected to ...
Subscription active (restored automatically after a reconnect).
Notification: 3
...
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [AutoReconnectClient.ino](AutoReconnectClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [NotifyServer](../NotifyServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

AutoReconnectClient - connect to the Gatt/NotifyServer example and subscribe once. With auto-reconnect on and persistent subscriptions (on by default), if the link drops the library reconnects to the same peer and restores the subscription on its own, so notifications resume with no extra code.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.setAutoReconnect(...)` — Configure retry policy explicitly; decide who owns reconnection so manual scans do not compete with automatic retries.
3. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
4. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
5. `ble.onSubscribed(...)` — Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
6. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
7. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
8. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
9. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Observe a full disconnect/reconnect cycle with the paired server. Persistent subscription restoration and reconnect policy are different mechanisms; inspect readiness after each connection rather than reusing an old connection ID.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
