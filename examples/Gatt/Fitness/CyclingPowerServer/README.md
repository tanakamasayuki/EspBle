# CyclingPowerServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Cycling Power Service (0x1818) peripheral. Cycling Power Measurement (0x2A63) is notified with 16-bit flags and a signed 16-bit instantaneous power (watts); Cycling Power Feature (0x2A65) and Sensor Location (0x2A5D) are readable.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [CyclingPowerClient](../CyclingPowerClient/) example, or any Cycling Power collector

## What it does

- Registers the Cycling Power service and advertises 0x1818
- Publishes Cycling Power Feature (0x0000000C) and Sensor Location = Right Crank (6) as readable values
- Every second, ramps the power 200..300 W (wrapping back to 200) and notifies a 4-byte Cycling Power Measurement with 16-bit flags 0x0000 (no optional fields)

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Cycling Power Measurement
- `ble.gattServer().setValue(...)` — seed the readable Feature / Sensor Location
- `ble.gattServer().notify(...)` — push each measurement to subscribers

## Notes

- Instantaneous power is a signed 16-bit little-endian value in watts, immediately after the 16-bit flags field.

## Expected Serial output

```
The server is silent; observe values on the client.
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [CyclingPowerServer.ino](CyclingPowerServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [CyclingPowerClient](../CyclingPowerClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

CyclingPowerServer - standard Cycling Power Service (0x1818). Cycling Power Measurement (0x2A63) is notified with 16-bit flags and a signed 16-bit instantaneous power (watts); Cycling Power Feature (0x2A65) and Sensor Location (0x2A5D) are readable.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
5. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.
6. `ble.gattServer().notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.

## Verify and adapt

Instantaneous power is signed watts. Preserve the sign, flags and little-endian order when replacing the sample; casting everything to an unsigned number can turn a negative value into a very large power.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
