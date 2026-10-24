# PrivateAddress

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 2, "GAP"

Advertises with a private address instead of the factory public address, selected via `EspBleConfig::ownAddressType`. A connectable peripheral example; observe the address type with the paired [Scan](../Scan/) example.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- A BLE scanner — the [Scan](../Scan/) example on a second board, or a scanner app such as nRF Connect

## The two modes

Switch with `USE_RESOLVABLE_PRIVATE_ADDRESS` at the top of the sketch.

| | RandomStatic (default) | ResolvablePrivate (RPA) |
|---|---|---|
| Address | A fixed random address generated at `begin()` | Rotated periodically by the controller |
| Tracking resistance | Hides the public address, but the fixed value is still trackable | Rotates, so it is hard to track |
| Bonding | Not needed | **Required**; the peer resolves the address with the IRK |
| Works standalone | Yes | No — without security the peer cannot reconnect |

The RPA rotation period is fixed by the bundled NimBLE's `CONFIG_BT_NIMBLE_RPA_TIMEOUT` (900 s) and cannot be changed from the application.

## What it does

- Sets `config.ownAddressType` for the selected mode; the RPA mode also enables `config.security.enabled` / `bonding`
- Advertises a connectable peripheral so a scanner can observe the address type
- Prints the peer address and bonded state on connect

## Key APIs

- `EspBleConfig::ownAddressType` — `Public` (default) / `RandomStatic` / `ResolvablePrivate`
- `EspBleConfig::security.enabled` / `bonding` — required when using an RPA
- `EspBleConnection::peerAddress` / `bonded` — the address as seen from this side, and the bonding state

## Notes

- `Public` — the factory public address. `RandomStatic` — a random static address generated at `begin()`; a fixed identity that does not rotate. `ResolvablePrivate` — a Resolvable Private Address (RPA) the controller rotates on its timer (`CONFIG_BT_NIMBLE_RPA_TIMEOUT`, 900 s on the bundled build); only useful with security/bonding, since a bonded peer resolves it via the IRK, while an unbonded scanner sees only a changing random address.
- A scanner sees this device with a **Random** address type (not Public). A static random address has its top two most-significant bits set (`0b11`) in the top octet.
- Extended/Periodic Advertising is not available: the bundled NimBLE is built with `CONFIG_BT_NIMBLE_EXT_ADV` off.

- The accept list matches by address, so a peer using an RPA cannot be listed until it is bonded and its identity address applies (see [AcceptList](../AcceptList/)).

## Expected Serial output

```
Advertising with a random static address.
Connected id=1 peer=d0:cf:13:58:fd:95 bonded=0
Disconnected id=1
```

## Related guides

- [BLE guide §2 GAP](../../../docs/GUIDE_BLE_BASICS.md#2-gap--finding-and-connecting) — advertising, scanning and connecting

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [PrivateAddress.ino](PrivateAddress.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [ScanDump](../../Info/ScanDump/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

PrivateAddress - advertise with a private address instead of the factory public address, so passers-by cannot track this device by its address. RandomStatic      = a fixed random address (hides the public one, never rotates) ResolvablePrivate = an RPA the controller rotates; a bonded peer resolves it with the IRK exchanged at bonding, everyone else sees a changing address Set USE_RESOLVABLE_PRIVATE_ADDRESS to switch. Observe with Gap/Scan or Info/ScanDump: the address type shows as Random either way.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.

## Verify and adapt

Compare static random and resolvable private addressing with the mode switch. Address privacy differs from payload encryption; retain the required security/bonding configuration when testing reconnection with a rotating address.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
