# Mtu

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 2, "GAP"

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
- `connection.mtu` — the MTU as of that event. **It is still 23 right after connecting**: the exchange happens just after the connection comes up, so `onConnected` sees the default and the negotiated value arrives through `onMtuChanged` (same order on both roles)
- `connection.maximumNotificationPayload()` — `mtu - 3` (ATT notification header)
- `ble.onMtuChanged(callback)` — `event.previousMtu` and `event.connection.mtu`

## Notes

- On the central side the MTU is a connection-time snapshot: a later change is not tracked. The bundled backend gives the GATT client no MTU-change notification, so EspBle has no way to learn about it.

## Expected Serial output

```
Connected with MTU 185 (notification payload up to 182 bytes)
```
