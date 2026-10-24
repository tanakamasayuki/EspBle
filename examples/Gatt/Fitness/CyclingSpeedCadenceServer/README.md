# CyclingSpeedCadenceServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Cycling Speed and Cadence Service (0x1816) peripheral. CSC Measurement (0x2A5B) is notified with cumulative wheel/crank revolutions and their last-event times; CSC Feature (0x2A5C) and Sensor Location (0x2A5D) are readable.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [CyclingSpeedCadenceClient](../CyclingSpeedCadenceClient/) example, or any CSC collector

## What it does

- Registers the CSC service and advertises 0x1816
- Publishes CSC Feature = Wheel + Crank (0x0003) and Sensor Location = Rear Hub (12) as readable values
- Every second, advances the counters (+2 wheel revs, +1 crank rev, +1.000 s event time) and notifies an 11-byte CSC Measurement with flags 0x03 (wheel + crank present)

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — CSC Measurement
- `ble.gattServer().setValue(...)` — seed the readable Feature / Sensor Location
- `ble.gattServer().notify(...)` — push each measurement to subscribers

## Notes

- Event times use 1/1024-second units; the sketch adds 1024 per notification.
- Wheel revolutions are a 32-bit field, crank revolutions a 16-bit field.

## Expected Serial output

```
The server is silent; observe values on the client.
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [CyclingSpeedCadenceServer.ino](CyclingSpeedCadenceServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [CyclingSpeedCadenceClient](../CyclingSpeedCadenceClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

CyclingSpeedCadenceServer - standard Cycling Speed and Cadence Service (0x1816). CSC Measurement (0x2A5B) is notified with cumulative wheel/crank revolutions and event times; CSC Feature (0x2A5C) and Sensor Location (0x2A5D) are readable.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
5. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.
6. `ble.gattServer().notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.

## Verify and adapt

Wheel/crank revolutions and event times are cumulative counters. Compute speed/cadence from changes between samples, account for wraparound and use the protocol time units; the displayed counters are not already km/h or rpm.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
