# RuntimePasskeyClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 3, "Security"

The **input side** of Passkey Entry (`KeyboardOnly`). It takes the 6 digits the peer displays **at runtime** and hands them over with `providePasskey()`. Its counterpart is [RuntimePasskeyServer](../RuntimePasskeyServer/).

[StaticPasskeyClient](../StaticPasskeyClient/) fixes the passkey in the sketch; this one follows what a real device does — the user reads it and types it in.

## Hardware

- 1 × ESP32-S3 running this sketch (central, input side)
- 1 × ESP32-S3 running [RuntimePasskeyServer](../RuntimePasskeyServer/)

## What it does

- Active-scans for the server's service UUID and connects to the first match
- `pairOnConnect` (on by default) starts pairing as soon as the connection comes up
- The stack asks for a passkey, and **pairing stops until it is answered**
- Sending `p` followed by 6 digits (e.g. `p481907`) calls `providePasskey()` and pairing resumes
- On success it discovers and reads a characteristic that requires `authenticatedRead`
- `c` deletes all bonds (only while disconnected)

## Key APIs

- `EspBleSecurityConfig::ioCapability = KeyboardOnly` — the side that types the passkey
- `ble.providePasskey(passkey)` — hand the 6 digits to the waiting pairing
- `ble.discoverCharacteristic(...)` / `ble.readCharacteristic(...)` — access after pairing

## Notes

- **Answer within 30 seconds.** Past that the stack stops waiting and pairing fails. Do not combine this with anything that blocks `loop()` for long.
- `providePasskey()` is accepted **either before or after** the request arrives; a value supplied early is used by the next request.
- A wrong value fails pairing and `onSecurityChanged` reports `success=0`. **There is no mechanism that reports which part was wrong** — allowing that would let an attacker try the passkey one digit at a time.
- On later connections the bond applies and no pairing happens, so nothing is asked for. Send `c` on both sides to try again.

## Expected Serial output

```
Send 'c' while disconnected to clear all bonds.
Connected id=1. Type p<passkey> (e.g. p123456) shown on the peer.
Passkey 481907 provided
Security established: encrypted=1 authenticated=1 bonded=1
Protected value: MITM protected value
```
