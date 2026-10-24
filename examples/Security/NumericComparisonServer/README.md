# NumericComparisonServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 3, "Security"

The peripheral side of **Numeric Comparison** pairing. Its counterpart is [NumericComparisonClient](../NumericComparisonClient/), and **both sides must be configured the same way** (`DisplayYesNo` plus MITM required).

The difference from Passkey Entry is that **neither side types anything**. LE Secure Connections shows the same 6 digits on both devices and the user only answers whether they match. This is the method used by devices that have a screen but no keyboard — a phone and a pair of earbuds, for instance.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × ESP32-S3 running [NumericComparisonClient](../NumericComparisonClient/)

Keep both serial monitors visible at once: **comparing the two numbers is the whole point** of this method.

## What it does

- Advertises as a GATT server whose characteristic requires `authenticatedRead`
- When pairing starts, the 6 digits to compare arrive at `onNumericComparison`
- `y` accepts the match, `n` rejects it. **Pairing is stopped until it is answered**
- Encryption only completes when both sides accept, and `onSecurityChanged` then reports `authenticated=1`
- `c` deletes all bonds (only while disconnected)

## Key APIs

- `EspBleSecurityConfig::ioCapability = DisplayYesNo` — can display and answer yes/no
- `ble.onNumericComparison(cb)` — the 6 digits to compare arrive in `event.passkey`
- `ble.confirmNumericComparison(accept)` — answer whether they matched

## Notes

- **Numeric Comparison is only chosen when both sides declare DisplayYesNo and both require MITM.** With `DisplayOnly` on one side you get Passkey Entry; with `None` you get Just Works. BLE has no API for selecting the method directly.
- **It requires LE Secure Connections**, so it cannot be used with peers older than BLE 4.2.
- **Answer within 30 seconds.** Past that the stack stops waiting and pairing fails.
- If the numbers differ, always send `n`. A mismatch is precisely the sign of a man-in-the-middle.

## Expected Serial output

```
Send 'c' while disconnected to clear all bonds.
Does the peer show 052913? Send 'y' to accept, 'n' to reject.
Answer accept: sent
Security established: encrypted=1 authenticated=1 bonded=1
```

## Related guides

- [BLE guide §3 Security](../../../docs/GUIDE_BLE_BASICS.md#3-security--how-far-to-trust-the-peer-you-connected-to) — pairing, bonding and IO capabilities

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [NumericComparisonServer.ino](NumericComparisonServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [NumericComparisonClient](../NumericComparisonClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

NumericComparisonServer - the peripheral half of Numeric Comparison pairing. Both sides declare DisplayYesNo and require MITM, so LE Secure Connections shows the same 6-digit value on both devices and each user confirms they match. Nobody types anything: the value is only ever compared.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `gattServer.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `gattServer.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `gattServer.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
5. `ble.onNumericComparison(...)` — Display the peer comparison value and obtain an explicit user decision; keep update running while waiting for Serial input.
6. `ble.onSecurityChanged(...)` — Read the security result and encrypted/authenticated/bonded flags; these describe different properties and must not be treated as interchangeable.
7. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
8. `advertising.addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.
9. `ble.confirmNumericComparison(...)` — Confirm only after comparing the displayed values on both peers; reject a mismatch rather than automatically accepting the event.
10. `ble.deleteAllBonds(...)` — Remove saved trust records deliberately; a restart alone does not erase them. Remove the corresponding record on the other peer when re-pairing.

## Verify and adapt

Accept only after the other peer displays the same number and the user confirms it. To repeat pairing from scratch, remove matching bonds on both peers while disconnected.

Pairing establishes security; bonding retains trust records for later connections. A reset does not necessarily repeat the pairing prompt. Test encrypted, authenticated and bonded states separately and keep update running while the user answers.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
