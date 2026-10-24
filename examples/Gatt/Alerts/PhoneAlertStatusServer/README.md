# PhoneAlertStatusServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Phone Alert Status Service (0x180E) peripheral. Alert Status (0x2A3F) and Ringer Setting (0x2A41) are read/**notify** uint8 values; the Ringer Control Point (0x2A40) is **Write Without Response**.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral, e.g. a phone proxy)
- 1 × central: the [PhoneAlertStatusClient](../PhoneAlertStatusClient/) example, or any Phone Alert Status client

## What it does

- Registers Alert Status, Ringer Setting, and Ringer Control Point before `begin()` and advertises 0x180E
- Starts with Alert Status 0x01 (Ringer State active) and Ringer Setting Normal (1)
- On a Ringer Control Point write, Set Silent Mode (1) switches Ringer Setting to Silent (0) and notifies; Cancel Silent Mode (3) switches it back to Normal (1)

## Key APIs

- `ble.gattServer().onWritten(...)` — receive Ringer Control Point commands
- `ble.gattServer().notify(...)` — notify the updated Ringer Setting

## Expected Serial output

```
Silent Mode
Normal Mode
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [PhoneAlertStatusServer.ino](PhoneAlertStatusServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [PhoneAlertStatusClient](../PhoneAlertStatusClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

PhoneAlertStatusServer - standard Phone Alert Status Service (0x180E). Alert Status (0x2A3F) and Ringer Setting (0x2A41) are read/notify uint8 values; the Ringer Control Point (0x2A40) is Write Without Response. Set Silent Mode (1) switches Ringer Setting to Silent and notifies; Cancel Silent Mode (3) switches it back to Normal.

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

Alert Status, Ringer Setting and Ringer Control Point are separate values. A control write changes the demonstration state; it does not connect this board to a phone call service.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
