# BondManagementServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Bond Management Service (0x181E) peripheral. Bond Management Feature (0x2AA5) is a readable uint24 bit field of supported operations; the Bond Management Control Point (0x2AA4) is writable and receives op codes in `onWritten`.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [BondManagementClient](../BondManagementClient/) example, or any Bond Management client

## What it does

- Registers the Control Point and Feature before `begin()` and advertises 0x181E
- Advertises support for "Delete bond of requesting device (LE)" (bit 0) and "Delete all bonds (LE + BR/EDR)" (bit 4) → 0x000011
- On op code 0x03 (Delete bond of requesting device, LE), removes that peer's bond after it disconnects (deferred so it does not disrupt the active link)

## Key APIs

- `ble.gattServer().onWritten(...)` — receive Control Point op codes
- `ble.bondCount()` / `ble.bond(i, bond)` / `ble.deleteBond(bond)` — bond enumeration and removal

## Expected Serial output

```
Bond Management op code: 3
Bond deleted
```
