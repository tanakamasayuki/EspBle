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

## Related guides

- [BLE guide §3 Security](../../../docs/GUIDE_BLE_BASICS.md#3-security--how-far-to-trust-the-peer-you-connected-to) — pairing, bonding and IO capabilities

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [RuntimePasskeyClient.ino](RuntimePasskeyClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [RuntimePasskeyServer](../RuntimePasskeyServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

RuntimePasskeyClient - the input side of Passkey Entry. It connects, pairing starts, and the stack asks for a passkey; the sketch answers with providePasskey() using a value typed into the serial monitor. That models a user reading the peer's display and typing what they see.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.onSecurityChanged(...)` — Read the security result and encrypted/authenticated/bonded flags; these describe different properties and must not be treated as interchangeable.
4. `ble.discoverCharacteristic(...)` — Find the requested remote characteristic and use the corresponding discovery completion to continue reading or writing.
5. `ble.onCharacteristicDiscovered(...)` — Continue only if discovery succeeded; a connection alone does not guarantee that the requested UUID exists.
6. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.
7. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
8. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
9. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
10. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Enter the passkey currently displayed by the server, not a value remembered from an earlier pairing. Keep processing update while waiting for input and observe the security-completion event.

Pairing establishes security; bonding retains trust records for later connections. A reset does not necessarily repeat the pairing prompt. Test encrypted, authenticated and bonded states separately and keep update running while the user answers.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
