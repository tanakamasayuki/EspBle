# StaticPasskeyClient

> 日本語版: [README.ja.md](README.ja.md)

Central-side counterpart of [StaticPasskeyServer](../StaticPasskeyServer/): the keyboard side (`KeyboardOnly`) of MITM-authenticated pairing. The current trial API passes a preconfigured passkey to the stack instead of waiting for runtime input, so the value here must match the one the server displays. After pairing succeeds it reads the characteristic that requires an authenticated link.

## Hardware

- 1 × ESP32-S3 running this sketch (central, passkey input side)
- 1 × ESP32-S3 running the [StaticPasskeyServer](../StaticPasskeyServer/) example

## What it does

- Scans for the server's service UUID and connects
- Starts pairing explicitly with `requestSecurity()` on connection
- Prints the security result (`authenticated=1` on success), then discovers and reads the MITM-protected characteristic
- Clears all bonds on `c` (while disconnected)

## Serial commands

| Command | Action |
|---------|--------|
| `c` | Delete all bonds and print the remaining count |

## Key APIs

- `EspBleSecurityConfig::ioCapability = KeyboardOnly` — this side "types" the passkey
- `config.security.staticPasskeyEnabled` / `staticPasskey` — preconfigured passkey (runtime passkey input is not implemented yet)
- `ble.requestSecurity(connectionId)` — explicit pairing start; completion arrives via `onSecurityChanged()`
- `authenticatedRead` characteristics are only readable after MITM pairing — a read before that fails with an ATT security error

## Expected Serial output

```
Send 'c' while disconnected to clear all bonds.
Security established: encrypted=1 authenticated=1 bonded=1
Protected value: MITM protected value
```
