# Mtu

> 日本語版: [README.ja.md](README.ja.md)

Requests a larger ATT MTU before connecting and observes the negotiated value. The preferred MTU is set in the config passed to `begin()`; the bundled NimBLE backend exchanges it during connection establishment.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × BLE peripheral — the sketch scans for the service UUID of the [Gatt/Basics/NotifyServer](../../Gatt/Basics/NotifyServer/) example, so run that on a second board

## What it does

- Sets `config.preferredMtu = 185` before `begin()`
- Connects to the first result advertising the NotifyServer service UUID
- Prints the negotiated MTU and the resulting maximum notification payload (`mtu - 3`)
- Prints MTU-change events with the previous and new value

## Key APIs

- `EspBleConfig::preferredMtu` — desired ATT MTU (23–517); out-of-range values are rejected by `begin()` with `InvalidArgument`
- `connection.mtu` — MTU snapshot taken when the connection completes
- `connection.maximumNotificationPayload()` — `mtu - 3` (ATT notification header)
- `ble.onMtuChanged(callback)` — `event.previousMtu` and `event.connection.mtu`

## Notes

- On the central side the MTU is a connection-time snapshot; the backend has no client-side MTU-change callback for later updates (see `docs/DECISIONS.ja.md`, Connection/GATT #23).

## Expected Serial output

```
Connected with MTU 185 (notification payload up to 182 bytes)
```
