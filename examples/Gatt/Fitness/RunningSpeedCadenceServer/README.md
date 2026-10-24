# RunningSpeedCadenceServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Running Speed and Cadence Service (0x1814) peripheral. RSC Measurement (0x2A53) is notified with instantaneous speed and cadence plus optional stride length and total distance; RSC Feature (0x2A54) and Sensor Location (0x2A5D) are readable.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [RunningSpeedCadenceClient](../RunningSpeedCadenceClient/) example, or any RSC collector

## What it does

- Registers the RSC service and advertises 0x1814
- Publishes RSC Feature = Stride + Distance (0x0003) and Sensor Location = In Shoe (2) as readable values
- Every second, notifies a 10-byte RSC Measurement (flags 0x03) at 3.0 m/s, cadence 180, stride 1.25 m, and a growing total distance (+3.0 m)

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — RSC Measurement
- `ble.gattServer().setValue(...)` — seed the readable Feature / Sensor Location
- `ble.gattServer().notify(...)` — push each measurement to subscribers

## Notes

- Speed is in 1/256 m/s units, stride length in 1/100 m, and total distance in 1/10 m. Flags bit 2 (walking/running) is left 0, so the reading is reported as walking.

## Expected Serial output

```
The server is silent; observe values on the client.
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [RunningSpeedCadenceServer.ino](RunningSpeedCadenceServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [RunningSpeedCadenceClient](../RunningSpeedCadenceClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

RunningSpeedCadenceServer - standard Running Speed and Cadence Service (0x1814). RSC Measurement (0x2A53) is notified with instantaneous speed and cadence plus optional stride length and total distance; RSC Feature (0x2A54) and Sensor Location (0x2A5D) are readable.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
5. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.
6. `ble.gattServer().notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.

## Verify and adapt

Flags control the optional stride-length and total-distance fields. Convert speed, cadence and distance according to their individual units; changing a flag also requires changing the payload length and decoder offsets.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
