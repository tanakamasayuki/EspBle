# PrivateAddress

> 日本語版: [README.ja.md](README.ja.md)

Advertises with a **private address** instead of the factory public address, using `EspBleConfig::ownAddressType`.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × BLE scanner (the [Gap/Scan](../Scan/) example on a second board, or a scanner app such as nRF Connect)

## What it does

- `config.ownAddressType = EspBleOwnAddressType::RandomStatic` — presents a fixed random static address that hides the public address
- Advertises a connectable peripheral so a scanner can observe the address type

## Address types

- `Public` (default) — the factory public address
- `RandomStatic` — a random static address generated at `begin()`; a fixed random identity that does not rotate
- `ResolvablePrivate` — a Resolvable Private Address (RPA) the controller rotates on its timer (`CONFIG_BT_NIMBLE_RPA_TIMEOUT`, 900 s on the bundled build). Only useful with security/bonding, since a bonded peer resolves it via the IRK exchanged at bonding; an unbonded scanner sees only a changing random address.

## Notes

- A scanner sees this device with a **Random** address type (not Public). A static random address has its two most-significant bits set (`0b11`) in the top octet.
- Extended/Periodic Advertising is not available: the bundled NimBLE is built with `CONFIG_BT_NIMBLE_EXT_ADV` off.
