# JustWorksServer

> 日本語版: [README.ja.md](README.ja.md)

A GATT server (peripheral) whose characteristic requires an encrypted link. Pairing uses Just Works (no passkey, LE Secure Connections) with bonding, started automatically on connection. Pairs with any BLE central (smartphone app such as nRF Connect, or another board).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / GATT server)
- 1 × BLE central that supports pairing (smartphone app or a second board)

## What it does

- Registers a characteristic with `encryptedRead` / `encryptedWrite` permissions before `begin()`
- Enables security with bonding and `pairOnConnect`, so pairing starts as soon as a central connects
- Prints the security result on `onSecurityChanged`: encrypted, authenticated (0 for Just Works), bonded, key size
- Prints any value written to the encrypted characteristic
- Restarts advertising after each disconnect
- Deletes all bonds on Serial command `c` (only allowed while disconnected) and prints the remaining count

## Key APIs

- `EspBleGattCharacteristicConfig::encryptedRead` / `encryptedWrite` — enforce an encrypted link at the ATT layer
- `EspBleSecurityConfig` — `enabled`, `bonding`, `pairOnConnect`
- `ble.onSecurityChanged(callback)` — `event.success` plus the connection security snapshot
- `ble.deleteAllBonds()` / `ble.bondCount()` — bond store management

## Notes

- Just Works pairing yields `encrypted=1` but `authenticated=0` (no MITM protection). Reading the characteristic before encryption fails with insufficient-encryption, which prompts the OS to pair.

## Expected Serial output

```
Send 'c' while disconnected to clear all bonds.
Security established: encrypted=1 authenticated=0 bonded=1 keySize=16
Encrypted write: hello
```
