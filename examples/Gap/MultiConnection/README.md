# MultiConnection

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md)

One Central holding several Peripheral connections at once. Each connection has
its own ID, and every operation names the connection it applies to — nothing in
this library is implicitly "the current connection".

## Hardware

- 1 × ESP32-S3 running this sketch (Central)
- 2–3 × peripherals advertising the Battery Service `0x180f`, for example boards
  running [Gatt/Device/BatteryServer](../../Gatt/Device/BatteryServer/)

## What it does

- Resumes scanning after each connection instead of stopping at the first peer,
  and stops collecting once it is full — a full Central would otherwise keep
  trying connections the host refuses
- Reads the battery level from every peer in turn; the shared result callback
  reports which connection each answer came from
- Removes the right entry on disconnect by matching the ID, which belongs to one
  connection for its whole life

## Key APIs

- `ble.connectionCount()` — how many are held right now
- `ble.readCharacteristic(connectionId, service, characteristic)` — the
  connection is part of the call
- `ble.onCharacteristicRead()` — one callback for every connection; the result
  carries the connection ID

## Limits

Three simultaneous connections on the original ESP32's bundled host. Other
targets allow more.

## Serial commands

| Key | Effect |
|---|---|
| `r` | read the battery level from every peer |
| `d` | disconnect the first peer |

## Related guides

- [BLE guide §2 GAP](../../../docs/GUIDE_BLE_BASICS.md#2-gap--finding-and-connecting) — advertising, scanning and connecting

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [MultiConnection.ino](MultiConnection.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

One Central holding several Peripheral connections at once. Each has its own connection ID, and every operation names the connection it applies to — nothing in this library is implicitly "the current connection".

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
4. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
5. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
6. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.
7. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.

## Verify and adapt

Keep state per connection ID and check the ID on reads, notifications and disconnects. One shared ready flag or one global peer handle is insufficient when several peers are connected.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
