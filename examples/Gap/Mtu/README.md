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

## Related guides

- [BLE guide §2 GAP](../../../docs/GUIDE_BLE_BASICS.md#2-gap--finding-and-connecting) — advertising, scanning and connecting

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [Mtu.ino](Mtu.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

Mtu - request a larger ATT MTU before connecting and observe the negotiated value. The preferred MTU is set in the config passed to begin(); the backend exchanges it during connection establishment.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
4. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Observe the negotiated MTU event rather than assuming the preferred MTU won. Available ATT value payload is smaller than the MTU; a larger MTU does not itself define message boundaries.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
