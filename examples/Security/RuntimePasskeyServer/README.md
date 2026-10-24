# RuntimePasskeyServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 3, "Security"

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

## Related guides

- [BLE guide §3 Security](../../../docs/GUIDE_BLE_BASICS.md#3-security--how-far-to-trust-the-peer-you-connected-to) — pairing, bonding and IO capabilities

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [RuntimePasskeyServer.ino](RuntimePasskeyServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [RuntimePasskeyClient](../RuntimePasskeyClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

RuntimePasskeyServer - the display side of Passkey Entry with a passkey that is generated per pairing instead of being fixed in the sketch. With no static passkey configured, the stack draws a fresh 6-digit value and hands it to onPasskeyDisplayed; a real product would show it on its screen.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `gattServer.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `gattServer.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `gattServer.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
5. `ble.onSecurityChanged(...)` — Read the security result and encrypted/authenticated/bonded flags; these describe different properties and must not be treated as interchangeable.
6. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
7. `advertising.addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.
8. `ble.deleteAllBonds(...)` — Remove saved trust records deliberately; a restart alone does not erase them. Remove the corresponding record on the other peer when re-pairing.

## Verify and adapt

Display the generated passkey for the current pairing so the client user can enter it. The display event is not pairing completion; inspect onSecurityChanged before trusting authenticated access.

Pairing establishes security; bonding retains trust records for later connections. A reset does not necessarily repeat the pairing prompt. Test encrypted, authenticated and bonded states separately and keep update running while the user answers.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
