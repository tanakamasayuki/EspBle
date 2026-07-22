# StaticPasskeyClient

> 日本語版: [README.ja.md](README.ja.md)

Central-side counterpart of [StaticPasskeyServer](../StaticPasskeyServer/): the passkey-input side (`KeyboardOnly`) of MITM-authenticated pairing. After pairing succeeds it discovers and reads the characteristic that requires an authenticated link.

## Hardware

- 1 × ESP32-S3 running this sketch (central, passkey input side)
- 1 × ESP32-S3 running the [StaticPasskeyServer](../StaticPasskeyServer/) example

## What it does

- Active-scans for the server's service UUID and connects to the first match
- Starts pairing explicitly with `requestSecurity()` on connection
- On security success, discovers the MITM-protected characteristic and reads it
- Prints the security result and the protected value
- Deletes all bonds on Serial command `c` (only allowed while disconnected) and prints the remaining count

## Key APIs

- `EspBleSecurityConfig::ioCapability = KeyboardOnly` — this side "types" the passkey
- `config.security.staticPasskeyEnabled` / `staticPasskey` — preconfigured passkey passed to the stack
- `ble.requestSecurity(connectionId)` — explicit pairing start; completion arrives via `onSecurityChanged()`
- `ble.discoverCharacteristic(...)` / `ble.readCharacteristic(...)` — access the characteristic after pairing

## Notes

- The current trial API passes the preconfigured passkey to the stack instead of waiting for runtime input, so `STATIC_PASSKEY` here must match the value the server displays.
- `authenticatedRead` characteristics are only readable after MITM pairing completes; a read before that fails with an ATT security error.

## Expected Serial output

```
Send 'c' while disconnected to clear all bonds.
Security established: encrypted=1 authenticated=1 bonded=1
Protected value: MITM protected value
```
