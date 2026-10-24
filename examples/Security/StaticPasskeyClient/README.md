# StaticPasskeyClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 3, "Security"

Central-side counterpart of [StaticPasskeyServer](../StaticPasskeyServer/): the passkey-input side (`KeyboardOnly`) of MITM-authenticated pairing. After pairing succeeds it discovers and reads the characteristic that requires an authenticated link.

Because the passkey is fixed in the sketch, nothing is typed at runtime. For the form where **the user actually types it**, see [RuntimePasskeyClient](../RuntimePasskeyClient/).

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

- The static passkey is handed to the stack up front, so `STATIC_PASSKEY` here must match the value the server displays. **It is compiled into the sketch and is therefore no secret from anyone who can read the source.** For production, prefer the [RuntimePasskeyServer](../RuntimePasskeyServer/) / [RuntimePasskeyClient](../RuntimePasskeyClient/) form.
- `authenticatedRead` characteristics are only readable after MITM pairing completes; a read before that fails with an ATT security error.

## Expected Serial output

```
Send 'c' while disconnected to clear all bonds.
Security established: encrypted=1 authenticated=1 bonded=1
Protected value: MITM protected value
```

## Related guides

- [BLE guide §3 Security](../../../docs/GUIDE_BLE_BASICS.md#3-security--how-far-to-trust-the-peer-you-connected-to) — pairing, bonding and IO capabilities

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [StaticPasskeyClient.ino](StaticPasskeyClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [StaticPasskeyServer](../StaticPasskeyServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

StaticPasskeyClient - central-side counterpart of StaticPasskeyServer: the keyboard side (KeyboardOnly) that "types" the passkey. The passkey is fixed in the sketch and handed to the stack up front, so nothing is typed at runtime; see Security/RuntimePasskeyClient for the form where the user enters it. After MITM-authenticated pairing it reads the protected characteristic.

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

The fixed passkey is demonstration configuration, not a secret suitable for every deployed device. Match the server configuration and verify authenticated security before accessing protected values.

Pairing establishes security; bonding retains trust records for later connections. A reset does not necessarily repeat the pairing prompt. Test encrypted, authenticated and bonded states separately and keep update running while the user answers.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
