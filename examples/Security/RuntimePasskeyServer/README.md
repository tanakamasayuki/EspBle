# RuntimePasskeyServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.ja.md) — chapter 3, "Security" (Japanese)

The **display side** of Passkey Entry (`DisplayOnly`), with a passkey that is **generated afresh by the stack for every pairing** instead of being fixed in the sketch. Its counterpart is [RuntimePasskeyClient](../RuntimePasskeyClient/).

The only difference from [StaticPasskeyServer](../StaticPasskeyServer/) is that `staticPasskeyEnabled` is left off — that alone makes the value change every time. **A static passkey is compiled into the sketch, so it is no secret from anyone who can read the source.** For a product with a screen, this is the form that was intended.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral, display side)
- 1 × ESP32-S3 running [RuntimePasskeyClient](../RuntimePasskeyClient/)

## What it does

- Advertises as a GATT server whose characteristic requires `authenticatedRead`
- When a central connects, pairing begins and the stack's generated 6-digit value arrives at `onPasskeyDisplayed`
- A human reads that value from the serial output and types it on the client side — it has to travel outside BLE, which is exactly what makes it MITM protection
- The result arrives at `onSecurityChanged`; on success `authenticated=1`
- `c` deletes all bonds (only while disconnected)

## Key APIs

- `EspBleSecurityConfig::ioCapability = DisplayOnly` — the side that shows the passkey
- **Not** setting `staticPasskeyEnabled` — this is what makes the passkey per-pairing
- `ble.onPasskeyDisplayed(cb)` — the 6 digits to show arrive in `event.passkey`

## Notes

- **Nothing is displayed on later connections.** A bonded peer encrypts with the stored key, so no pairing happens at all. To see it again, delete the bonds on **both** sides (`c`).
- The value is delivered in `update()` context. SMP is stopped, waiting for the answer, the whole time.

## Expected Serial output

```
Send 'c' while disconnected to clear all bonds.
Enter passkey 481907 on the peer.
Security established: encrypted=1 authenticated=1 bonded=1
```
