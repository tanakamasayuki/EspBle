# JustWorksServer

> 日本語版: [README.ja.md](README.ja.md)

A GATT server whose characteristic requires an encrypted link. Pairing uses Just Works (no passkey, LE Secure Connections) with bonding, started automatically on connection. Connect from a smartphone (nRF Connect etc.) or another board; reading the characteristic before encryption fails with an insufficient-encryption error, which prompts the OS to pair.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / GATT server)
- 1 × BLE central that supports pairing (smartphone app or a second board)

## What it does

- Registers a characteristic with `encryptedRead` / `encryptedWrite` permissions before `begin()`
- Enables security with bonding and `pairOnConnect` (pairing starts as soon as a central connects)
- Prints the security result: encrypted, authenticated (false for Just Works), bonded, key size
- Restarts advertising after each disconnect
- Clears all bonds when `c` is received on Serial (only allowed while disconnected)

## Serial commands

| Command | Action |
|---------|--------|
| `c` | Delete all bonds and print the remaining count |

## Key APIs

- `EspBleGattCharacteristicConfig::encryptedRead` / `encryptedWrite` — enforce Security Mode 1 Level 2 at the ATT layer
- `EspBleSecurityConfig` — `enabled`, `bonding`, `pairOnConnect`
- `ble.onSecurityChanged(callback)` — `event.success` plus the connection security snapshot
- `ble.deleteAllBonds()` / `ble.bondCount()` — bond store management

## Expected Serial output

```
Send 'c' while disconnected to clear all bonds.
Security established: encrypted=1 authenticated=0 bonded=1 keySize=16
Encrypted write: ...
```
