# ReferenceTimeUpdateServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Reference Time Update Service (0x1806) peripheral. Time Update Control Point (0x2A16) is **Write Without Response** (1 = Get Reference Update, 2 = Cancel Reference Update); Time Update State (0x2A17) is a readable 2-byte value (Current State + Result).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [ReferenceTimeUpdateClient](../ReferenceTimeUpdateClient/) example, or any Reference Time Update client

## What it does

- Registers the Control Point and State before `begin()` and advertises 0x1806
- Starts Idle with a Successful result (0, 0)
- On a Control Point write, Get Reference Update (1) transitions State to Update Pending (1, 0); Cancel Reference Update (2) returns it to Idle with a Canceled result (0, 1)

## Key APIs

- `ble.gattServer().onWritten(...)` — receive Control Point commands
- `ble.gattServer().setValue(...)` — update the read-only State

## Expected Serial output

```
Get Reference Update
Cancel Reference Update
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [ReferenceTimeUpdateServer.ino](ReferenceTimeUpdateServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [ReferenceTimeUpdateClient](../ReferenceTimeUpdateClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

ReferenceTimeUpdateServer - standard Reference Time Update Service (0x1806). Time Update Control Point (0x2A16) is Write Without Response (1 = Get Reference Update, 2 = Cancel Reference Update); Time Update State (0x2A17) is a readable 2-byte value (Current State + Result). Control Point writes transition the read-only state.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.gattServer().setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
2. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
3. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
4. `server.onWritten(...)` — Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
5. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
6. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

Get/Cancel commands change the reported update state. The example does not fetch network time: connect your own time source before reporting that a real reference update succeeded.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
