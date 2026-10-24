# FitnessMachineServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Fitness Machine Service (0x1826) peripheral, as used by smart trainers and indoor bikes. Indoor Bike Data (0x2AD2) is **notified** with 16-bit flags followed by instantaneous speed (0.01 km/h), cadence (0.5 /min), and signed power (W); Fitness Machine Feature (0x2ACC) is a readable 8-byte pair of feature bitmaps.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [FitnessMachineClient](../FitnessMachineClient/) example, or any Fitness Machine collector (Zwift, etc.)

## What it does

- Registers the Fitness Machine service before `begin()` and advertises 0x1826
- Every second, notifies Indoor Bike Data with speed ramping 30–40 km/h, 90 rpm cadence, and 250 W power (flags 0x0044)

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Indoor Bike Data
- `ble.gattServer().notify(...)` — notification to subscribers

## Notes

- Indoor Bike Data bit 0 is *More Data*: instantaneous speed is present when it is **0**. Optional fields follow the flag order (average speed, cadence, distance, resistance, power, …).
- This example demonstrates the data-broadcast path; the Fitness Machine Control Point (interactive target/resistance control) is not implemented.

## Expected Serial output

The server is silent; observe the decoded values on the client.

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [FitnessMachineServer.ino](FitnessMachineServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [FitnessMachineClient](../FitnessMachineClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

FitnessMachineServer - standard Fitness Machine Service (0x1826) as used by smart trainers / indoor bikes. Indoor Bike Data (0x2AD2) is notified with 16-bit flags followed by instantaneous speed (0.01 km/h), cadence (0.5 /min), and signed power (W); Fitness Machine Feature (0x2ACC) is a readable 8-byte pair of feature bitmaps.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
5. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.
6. `ble.gattServer().notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.

## Verify and adapt

Indoor Bike Data is flags-driven: speed, cadence and power have different scales and optional positions. This example exchanges sample bike data; it does not implement a complete exercise-machine control system.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
