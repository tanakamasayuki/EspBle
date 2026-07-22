# StaticPasskeyServer

> 日本語版: [README.ja.md](README.ja.md)

A GATT server (peripheral) requiring MITM-authenticated pairing with a static 6-digit passkey. This board is the display side (`DisplayOnly`): it prints the passkey and the connecting central types it. Pairs with the [StaticPasskeyClient](../StaticPasskeyClient/) example (the passkey-input side).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / GATT server, passkey display side)
- 1 × BLE central with keyboard input: the [StaticPasskeyClient](../StaticPasskeyClient/) example or a smartphone app

## What it does

- Registers a characteristic with `authenticatedRead` / `authenticatedWrite` permissions before `begin()`
- Enables security with `mitm`, `ioCapability = DisplayOnly`, and the static passkey `438209`
- Prints the passkey on `onPasskeyDisplayed` when pairing starts
- Prints the security result on `onSecurityChanged` (`authenticated=1` on success)
- Restarts advertising after each disconnect

## Key APIs

- `EspBleGattCharacteristicConfig::authenticatedRead` / `authenticatedWrite` — require a MITM-authenticated link
- `EspBleSecurityConfig` — `mitm`, `ioCapability` (`DisplayOnly` / `KeyboardOnly`), `staticPasskeyEnabled`, `staticPasskey`
- `ble.onPasskeyDisplayed(callback)` — delivered from `ble.update()`; the passkey to present to the user
- `ble.onSecurityChanged(callback)` — check `event.connection.authenticated`

## Notes

- The characteristic requires an authenticated link, so Just Works pairing cannot access it.
- The passkey is a fixed example value. Production devices should provision a unique passkey per device instead of hard-coding a shared one.

## Expected Serial output

```
Enter passkey 438209 on the peer.
Security established: encrypted=1 authenticated=1 bonded=1
```
