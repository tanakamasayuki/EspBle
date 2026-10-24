# BondManagementServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Bond Management Service (0x181E) peripheral. Bond Management Feature (0x2AA5) is a readable uint24 bit field of supported operations; the Bond Management Control Point (0x2AA4) is writable and receives op codes in `onWritten`.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [BondManagementClient](../BondManagementClient/) example, or any Bond Management client

## What it does

- Registers the Control Point and Feature before `begin()` and advertises 0x181E
- Advertises support for "Delete bond of requesting device (LE)" (bit 0) and "Delete all bonds (LE + BR/EDR)" (bit 4) → 0x000011
- On op code 0x03 (Delete bond of requesting device, LE), removes that peer's bond after it disconnects (deferred so it does not disrupt the active link)

## Key APIs

- `ble.gattServer().onWritten(...)` — receive Control Point op codes
- `ble.bondCount()` / `ble.bond(i, bond)` / `ble.deleteBond(bond)` — bond enumeration and removal

## Expected Serial output

```
Bond Management op code: 3
Bond deleted
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [BondManagementServer.ino](BondManagementServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [BondManagementClient](../BondManagementClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

BondManagementServer - standard Bond Management Service (0x181E). Bond Management Feature (0x2AA5) is a readable uint24 bit field of supported operations; the Bond Management Control Point (0x2AA4) is writable and receives op codes in onWritten. On "Delete bond of requesting device" (0x03), the server removes that peer's bond after it disconnects.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `server.onWritten(...)` — Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
5. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
6. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
7. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
8. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

The Control Point changes stored bonds rather than merely changing a displayed setting. Inspect the supported Feature bits and the exact opcode implemented in the sketch; do not treat the example as authorization policy for an exposed management service.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
