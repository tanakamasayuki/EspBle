# WeightScaleServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Weight Scale Service (0x181D) peripheral. Weight Measurement (0x2A9D) is **indicated** as a uint16 weight at 0.005 kg resolution; Weight Scale Feature (0x2A9E) is a readable uint32.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [WeightScaleClient](../WeightScaleClient/) example, or any Weight Scale collector

## What it does

- Registers the Weight Scale service before `begin()` and advertises 0x181D
- Every 3 seconds, indicates a weight around 70 kg (raw uint16, 0.005 kg/LSB)
- `indicate()` only reaches subscribers

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .indicatable = true })` — Weight Measurement
- `ble.gattServer().indicate(...)` — acknowledged indication

## Expected Serial output

The server is silent; observe the weight on the client.

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [WeightScaleServer.ino](WeightScaleServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [WeightScaleClient](../WeightScaleClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

WeightScaleServer - standard Weight Scale Service (0x181D). Weight Measurement (0x2A9D) is indicated as a uint16 at 0.005 kg resolution; Weight Scale Feature (0x2A9E) is a readable uint32.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
5. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.
6. `ble.gattServer().indicate(...)` — Queue an indication for subscribers. It has an ATT confirmation, but immediate acceptance still precedes completion and does not prove the peer application performed an action.

## Verify and adapt

Weight Measurement is an integer with flags selecting units and optional fields. Apply the protocol resolution to the raw integer before displaying a human-readable weight.

The server values are simulated protocol examples, not measurements from a medical sensor. Replace data acquisition separately while preserving the service encoding and validity checks on the client.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
