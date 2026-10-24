# HealthThermometerServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Health Thermometer Service (0x1809) peripheral. Temperature Measurement (0x2A1C) is **indicated** as an IEEE-11073 32-bit FLOAT; Temperature Type (0x2A1D) is readable.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [HealthThermometerClient](../HealthThermometerClient/) example, or any Health Thermometer collector

## What it does

- Registers the Health Thermometer service before `begin()` and advertises 0x1809
- Every 2 seconds, indicates a slowly rising temperature (36.50–38.49 °C) encoded as an IEEE-11073 FLOAT
- `indicate()` only reaches subscribers, so it is a no-op until a client subscribes

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .indicatable = true })` — Temperature Measurement
- `espBleWriteMedicalFloat32LE(out, mantissa, exponent)` — IEEE-11073 32-bit FLOAT (from `EspBleMedicalFloat.h`)
- `ble.gattServer().indicate(service, characteristic, data, length)` — acknowledged indication

## Expected Serial output

The server is silent; observe the temperature on the client.

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HealthThermometerServer.ino](HealthThermometerServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [HealthThermometerClient](../HealthThermometerClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

HealthThermometerServer - standard Health Thermometer Service (0x1809). Temperature Measurement (0x2A1C) is indicated as an IEEE-11073 32-bit FLOAT; Temperature Type (0x2A1D) is readable. Pairs with HealthThermometerClient or any Health Thermometer collector.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
5. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.
6. `ble.gattServer().indicate(...)` — Queue an indication for subscribers. It has an ATT confirmation, but immediate acceptance still precedes completion and does not prove the peer application performed an action.

## Verify and adapt

Temperature Measurement uses the medical FLOAT encoding and a flags-selected unit. Use the supplied conversion helpers rather than casting packet bytes to a float pointer.

The server values are simulated protocol examples, not measurements from a medical sensor. Replace data acquisition separately while preserving the service encoding and validity checks on the client.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
