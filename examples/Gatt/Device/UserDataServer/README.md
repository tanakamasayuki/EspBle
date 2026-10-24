# UserDataServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard User Data Service (0x181C) peripheral. Age (0x2A80) is a read/write uint8, First Name (0x2A8A) is a read/write utf8s, and Database Change Increment (0x2A99) is a read/write/**notify** uint32. Each write to Age or First Name bumps the increment and notifies it.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [UserDataClient](../UserDataClient/) example, or any User Data collector

## What it does

- Registers Age, First Name, and Database Change Increment before `begin()` and advertises 0x181C
- Handles client writes in `onWritten`: stores the new Age, logs the First Name, and bumps + notifies the Database Change Increment
- Starts with Age 25 and increment 0

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .readable = true, .writable = true })` — writable characteristics
- `ble.gattServer().onWritten(...)` — receive client writes in loop context
- `ble.gattServer().notify(...)` — notify the updated Database Change Increment

## Expected Serial output

```
First Name updated: Ada
Age updated: 42
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [UserDataServer.ino](UserDataServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [UserDataClient](../UserDataClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

UserDataServer - standard User Data Service (0x181C). Age (0x2A80) is a read/write uint8, First Name (0x2A8A) is a read/write utf8s, and Database Change Increment (0x2A99) is a read/write/notify uint32. Each write to Age or First Name bumps the increment and notifies it.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.gattServer().setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
2. `ble.gattServer().notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.
3. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
4. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
5. `server.onWritten(...)` — Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
6. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
7. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

Age, First Name and Database Change Increment use different encodings. Preserve the increment update when changing writable fields so subscribers can detect a database change; the example values are demonstration data.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
