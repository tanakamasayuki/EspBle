# StaticPasskeyServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 3, "Security"

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

## Related guides

- [BLE guide §3 Security](../../../docs/GUIDE_BLE_BASICS.md#3-security--how-far-to-trust-the-peer-you-connected-to) — pairing, bonding and IO capabilities

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [StaticPasskeyServer.ino](StaticPasskeyServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [StaticPasskeyClient](../StaticPasskeyClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

StaticPasskeyServer - a GATT server requiring MITM-authenticated pairing with a static 6-digit passkey. This board is the display side (DisplayOnly): it prints the passkey and the connecting central types it. The characteristic requires an authenticated link, so Just Works pairing cannot access it.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `gattServer.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `gattServer.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `gattServer.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
5. `ble.onSecurityChanged(...)` — Read the security result and encrypted/authenticated/bonded flags; these describe different properties and must not be treated as interchangeable.
6. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
7. `advertising.addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

A static passkey must match the client and remains the same across pairings. When adapting the example, choose the intended provisioning/authentication policy rather than distributing the sample value as a shared production secret.

Pairing establishes security; bonding retains trust records for later connections. A reset does not necessarily repeat the pairing prompt. Test encrypted, authenticated and bonded states separately and keep update running while the user answers.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
