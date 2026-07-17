# StaticPasskeyServer

> 日本語版: [README.ja.md](README.ja.md)

A GATT server that requires MITM-authenticated pairing with a static 6-digit passkey. The board acts as the display side (`DisplayOnly`): it prints the passkey and the connecting central must type it. The characteristic requires an authenticated link, so Just Works pairing is not sufficient to access it.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / GATT server, passkey display side)
- 1 × BLE central with keyboard input (smartphone app or a second board)

## What it does

- Registers a characteristic with `authenticatedRead` / `authenticatedWrite` permissions before `begin()`
- Enables security with `mitm`, `ioCapability = DisplayOnly`, and the static passkey `438209`
- Prints the passkey when pairing starts, and the security result when it completes (`authenticated=1` on success)
- Restarts advertising after each disconnect

The passkey is a fixed example value. Production devices should provision a unique passkey per device instead of hard-coding a shared one.

## Key APIs

- `EspBleGattCharacteristicConfig::authenticatedRead` / `authenticatedWrite` — require a MITM-authenticated link
- `EspBleSecurityConfig` — `mitm`, `ioCapability` (`DisplayOnly` / `KeyboardOnly`), `staticPasskeyEnabled`, `staticPasskey`
- `ble.onPasskeyDisplayed(callback)` — delivered from `ble.update()`; shows the passkey to present to the user
- `ble.onSecurityChanged(callback)` — check `event.connection.authenticated`

## Expected Serial output

```
Enter passkey 438209 on the peer.
Security established: encrypted=1 authenticated=1 bonded=1
```
