# JustWorksServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 3, "Security"

A GATT server (peripheral) whose characteristic requires an encrypted link. Pairing uses Just Works (no passkey, LE Secure Connections) with bonding, started automatically on connection. Pairs with any BLE central (smartphone app such as nRF Connect, or another board).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / GATT server)
- 1 × BLE central that supports pairing (smartphone app or a second board)

## What it does

- Registers a characteristic with `encryptedRead` / `encryptedWrite` permissions before `begin()`
- Enables security with bonding and `pairOnConnect`, so pairing starts as soon as a central connects
- Prints the security result on `onSecurityChanged`: encrypted, authenticated (0 for Just Works), bonded, key size
- Prints any value written to the encrypted characteristic
- Restarts advertising after each disconnect
- Deletes all bonds on Serial command `c` (only allowed while disconnected) and prints the remaining count

## Key APIs

- `EspBleGattCharacteristicConfig::encryptedRead` / `encryptedWrite` — enforce an encrypted link at the ATT layer
- `EspBleSecurityConfig` — `enabled`, `bonding`, `pairOnConnect`
- `ble.onSecurityChanged(callback)` — `event.success` plus the connection security snapshot
- `ble.deleteAllBonds()` / `ble.bondCount()` — bond store management

## Notes

- Just Works pairing yields `encrypted=1` but `authenticated=0` (no MITM protection). Reading the characteristic before encryption fails with insufficient-encryption, which prompts the OS to pair.

## Expected Serial output

```
Send 'c' while disconnected to clear all bonds.
Security established: encrypted=1 authenticated=0 bonded=1 keySize=16
Encrypted write: hello
```

## Related guides

- [BLE guide §3 Security](../../../docs/GUIDE_BLE_BASICS.md#3-security--how-far-to-trust-the-peer-you-connected-to) — pairing, bonding and IO capabilities

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [JustWorksServer.ino](JustWorksServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

JustWorksServer - a GATT server whose characteristic requires an encrypted link. Pairing uses Just Works (no passkey, LE Secure Connections) + bonding, started automatically on connection. Reading before encryption returns insufficient- encryption, which prompts the OS to pair.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `gattServer.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `gattServer.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `gattServer.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `gattServer.onWritten(...)` — Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
5. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
6. `ble.onSecurityChanged(...)` — Read the security result and encrypted/authenticated/bonded flags; these describe different properties and must not be treated as interchangeable.
7. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
8. `advertising.addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.
9. `ble.deleteAllBonds(...)` — Remove saved trust records deliberately; a restart alone does not erase them. Remove the corresponding record on the other peer when re-pairing.

## Verify and adapt

Just Works can encrypt a link without authenticating against a man-in-the-middle. It has no passkey comparison step; use an authenticated pairing example if the application needs that property.

Pairing establishes security; bonding retains trust records for later connections. A reset does not necessarily repeat the pairing prompt. Test encrypted, authenticated and bonded states separately and keep update running while the user answers.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
