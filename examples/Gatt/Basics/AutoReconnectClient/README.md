# AutoReconnectClient

> 日本語版: [README.ja.md](README.ja.md)

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
