# CurrentTimeServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Current Time Service (0x1805) peripheral. Current Time (0x2A2B) is a read/**notify** characteristic holding the standard 10-byte wire format (year LE, month, day, hours, minutes, seconds, weekday, fraction256, adjust reason).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [CurrentTimeClient](../CurrentTimeClient/) example, or any Current Time client

## What it does

- Registers Current Time before `begin()` and advertises 0x1805
- Seeds the value with a fixed demo time (2026-07-19 12:34:56, Sunday, manually adjusted)
- Send `t` over Serial to advance the seconds field by one, then set the value and notify subscribers

## Key APIs

- `ble.gattServer().setValue(...)` — update the stored Current Time value
- `ble.gattServer().notify(...)` — notify subscribers of the new time

## Expected Serial output

```
Send 't' to advance one second and notify subscribers.
Time: 2026-07-19 12:34:57 (notification accepted: 1)
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [CurrentTimeServer.ino](CurrentTimeServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [CurrentTimeClient](../CurrentTimeClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
2. `server.notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.
3. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
4. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
5. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
6. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

Current Time is a structured date/time value, not Unix seconds. Keep calendar fields, day of week, fractions and adjustment reason coherent; the sample is not an RTC or a time synchronization source.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
