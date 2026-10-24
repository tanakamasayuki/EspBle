# NumericComparisonClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 3, "Security"

The central side of **Numeric Comparison** pairing. Its counterpart is [NumericComparisonServer](../NumericComparisonServer/), and the configuration is **exactly the same as the server's** (`DisplayYesNo` plus MITM required) — both sides declaring the same thing is what makes this method get chosen.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × ESP32-S3 running [NumericComparisonServer](../NumericComparisonServer/)

Keep both serial monitors visible at once.

## What it does

- Active-scans for the server's service UUID and connects to the first match
- `pairOnConnect` (on by default) starts pairing as soon as the connection comes up
- The 6 digits to compare arrive at `onNumericComparison` — they should equal what the server shows
- `y` accepts, `n` rejects. **Pairing is stopped until it is answered**
- Once both sides accept, it discovers and reads a characteristic requiring `authenticatedRead`
- `c` deletes all bonds (only while disconnected)

## Key APIs

- `EspBleSecurityConfig::ioCapability = DisplayYesNo`
- `ble.onNumericComparison(cb)` / `ble.confirmNumericComparison(accept)`
- `ble.discoverCharacteristic(...)` / `ble.readCharacteristic(...)` — access after pairing

## Notes

- **A rejection from either side fails the pairing.** One `n` ends it for both.
- **Answer within 30 seconds**, or the stack stops waiting.
- On later connections the bond applies and no pairing happens, so nothing is asked. Send `c` on both sides to try again.

## Expected Serial output

```
Send 'c' while disconnected to clear all bonds.
Does the peer show 052913? Send 'y' to accept, 'n' to reject.
Answer accept: sent
Security established: encrypted=1 authenticated=1 bonded=1
Protected value: MITM protected value
```

## Related guides

- [BLE guide §3 Security](../../../docs/GUIDE_BLE_BASICS.md#3-security--how-far-to-trust-the-peer-you-connected-to) — pairing, bonding and IO capabilities

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [NumericComparisonClient.ino](NumericComparisonClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [NumericComparisonServer](../NumericComparisonServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

NumericComparisonClient - the central half of Numeric Comparison pairing. It declares DisplayYesNo and requires MITM, exactly like the server, so the same 6-digit value appears on both. After both users confirm, it reads a characteristic that only an authenticated link may read.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onNumericComparison(...)` — Display the peer comparison value and obtain an explicit user decision; keep update running while waiting for Serial input.
3. `ble.onSecurityChanged(...)` — Read the security result and encrypted/authenticated/bonded flags; these describe different properties and must not be treated as interchangeable.
4. `ble.discoverCharacteristic(...)` — Find the requested remote characteristic and use the corresponding discovery completion to continue reading or writing.
5. `ble.onCharacteristicDiscovered(...)` — Continue only if discovery succeeded; a connection alone does not guarantee that the requested UUID exists.
6. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.
7. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
8. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
9. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
10. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Wait for both peers to display the same number, then answer the Serial prompt on each side. A connection or an encrypted flag alone does not prove the authenticated Numeric Comparison procedure succeeded.

Pairing establishes security; bonding retains trust records for later connections. A reset does not necessarily repeat the pairing prompt. Test encrypted, authenticated and bonded states separately and keep update running while the user answers.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
