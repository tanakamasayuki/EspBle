# ImmediateAlertServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Immediate Alert Service (0x1802) peripheral — the Find Me profile **target** role. Alert Level (0x2A06) is a single **Write Without Response** uint8 (0 = No Alert, 1 = Mild, 2 = High).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / Find Me target)
- 1 × central: the [ImmediateAlertClient](../ImmediateAlertClient/) example, or any Find Me locator

## What it does

- Registers Alert Level as a write / write-without-response characteristic before `begin()` and advertises 0x1802
- Reacts to each written Alert Level in `onWritten` (a real target would beep or vibrate)

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .writable = true, .writableWithoutResponse = true })`
- `ble.gattServer().onWritten(...)` — receive Alert Level writes in loop context

## Expected Serial output

```
Alert Level: 2 (High Alert)
Alert Level: 0 (No Alert)
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [ImmediateAlertServer.ino](ImmediateAlertServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [ImmediateAlertClient](../ImmediateAlertClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

ImmediateAlertServer - standard Immediate Alert Service (0x1802), the Find Me profile target role. Alert Level (0x2A06) is a single Write Without Response uint8 (0 = No Alert, 1 = Mild, 2 = High). The server reacts to each written level in onWritten.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.onWritten(...)` — Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
4. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
5. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

Alert Level is a command value sent with Write Without Response. Receiving or logging the command does not drive a buzzer; add your output action separately and define how it is stopped.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
