# ConnectionInspector

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 2, "GAP"

Interactive diagnostic tool. It lists nearby connectable devices with index numbers, connects to the one you pick, and dumps the full connection snapshot: connection ID, backend handle, peer address and type, local role, negotiated MTU (and the resulting notification payload limit), and the security state (encrypted / authenticated / bonded / key size). It can also dump the bond store and the library's diagnostic counters.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- Nearby BLE peripherals to inspect (any advertising device)

## What it does

- Scans and lists up to 10 unique connectable devices as `[index] address rssi name`
- `0`–`9` connects to the listed device with that index and prints its connection snapshot
- `s` clears the list and rescans; `d` disconnects the current connection; `b` dumps the bond store; `q` prints the diagnostic counters
- Security is disabled here, so peripherals that require encryption still accept the connection and show their link info, but reject attribute access

## Key APIs

- `EspBleConnection` — `id`, `handle`, `peerAddress`, `peerAddressType`, `localRole`, `mtu`, `maximumNotificationPayload()`, `encrypted`, `authenticated`, `bonded`, `encryptionKeySize`
- `ble.connect(scanResult)` / `ble.disconnect(connectionId)` / `ble.onConnectionFailed(callback)`
- `ble.bondCount()` / `ble.bond(index, out)` — snapshot access to the bond store
- `ble.connectionCount()`, `ble.droppedEventCount()`, `ble.scanner().droppedResultCount()`

## Expected Serial output

```
Commands: 0-9 connect to listed device, s rescan, d disconnect, b bonds, q counters
SCAN restart success=1 - send the list number to connect
[0] 5a:b8:1e:0c:2f:71 rssi=-52 name=EspBle Keyboard
CONNECT [0] 5a:b8:1e:0c:2f:71 accepted=1
CONNECTION id=1 handle=0 peer=5a:b8:1e:0c:2f:71(type=0) role=Central
  mtu=255 maxNotificationPayload=252
  encrypted=0 authenticated=0 bonded=0 keySize=0
```

## Related guides

- [BLE guide §1 What BLE is](../../../docs/GUIDE_BLE_BASICS.md#1-what-ble-is) — the vocabulary the rest of the docs use

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [ConnectionInspector.ino](ConnectionInspector.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

ConnectionInspector - interactive diagnostic tool: lists nearby connectable devices, connects to the one you pick, and dumps the connection snapshot (MTU, roles, security state). Also prints the bond store and diagnostic counters. Security is disabled here, so peripherals that require encryption reject reads but still accept the connection and show their link info.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
4. `ble.onConnectionFailed(...)` — Observe failures that happen after a connection request was accepted, including an unavailable peer or a rejected connection.
5. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
6. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Inspect each connection and the stored bond list separately. Watch dropped-event counters while increasing activity; a device disappearing from the active connections does not erase its bond.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
