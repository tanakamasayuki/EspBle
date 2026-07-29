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
