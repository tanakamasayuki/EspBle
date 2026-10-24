# HeartRateServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Heart Rate Service (0x180D) peripheral. Heart Rate Measurement (0x2A37) is **notify-only** and carries an 8-bit heart rate plus one RR interval; Body Sensor Location (0x2A38) is readable (value 1 = Chest).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [HeartRateClient](../HeartRateClient/) example, or any Heart Rate collector

## What it does

- Registers Heart Rate Measurement (notify) and Body Sensor Location (read) before `begin()` and advertises 0x180D
- Sends `+` / `-` over Serial to increase/decrease the heart rate (1–250 bpm) and notify subscribers
- Emits a variable-length measurement: flags 0x10 (RR interval present), 8-bit bpm, one RR interval

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Heart Rate Measurement
- `ble.gattServer().setValue(...)` — update the measurement / Body Sensor Location value
- `ble.gattServer().notify(service, characteristic, data, length)` — notify subscribers

## Expected Serial output

```
Send '+' or '-' to change the heart rate and notify subscribers.
Heart rate: 71 bpm (notification accepted: 1)
Heart rate: 72 bpm (notification accepted: 1)
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HeartRateServer.ino](HeartRateServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [HeartRateClient](../HeartRateClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
2. `server.notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.
3. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
4. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
5. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
6. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

The flags select the heart-rate field width and optional data. Do not assume every Heart Rate Measurement is exactly one flags byte plus one rate byte.

The server values are simulated protocol examples, not measurements from a medical sensor. Replace data acquisition separately while preserving the service encoding and validity checks on the client.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
