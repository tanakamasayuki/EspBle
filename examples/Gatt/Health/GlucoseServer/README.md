# GlucoseServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Glucose Service (0x1808) peripheral with the **Record Access Control Point (RACP)** procedure. When a client writes "Report Stored Records (all)", the server notifies one Glucose Measurement and then indicates the RACP response.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [GlucoseClient](../GlucoseClient/) example, or any Glucose collector

## What it does

- Registers Glucose Measurement (0x2A18, notify), Glucose Feature (0x2A51, read), and RACP (0x2A52, write + indicate)
- On a RACP "Report Stored Records" write, notifies one measurement (sequence number, base time, SFLOAT concentration, type/location)
- Sequences the measurement notify and the RACP response indicate from `onSent` (sends queue, so this ordering follows the local send-result event)

## Key APIs

- `ble.gattServer().onWritten(...)` — receive the RACP request
- `ble.gattServer().onSent(...)` — sequence the notify → indicate procedure
- `espBleWriteMedicalSFloatLE(...)` — IEEE-11073 16-bit SFLOAT concentration

## Expected Serial output

The server is silent; observe the records on the client.

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [GlucoseServer.ino](GlucoseServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [GlucoseClient](../GlucoseClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

GlucoseServer - standard Glucose Service (0x1808) with the Record Access Control Point (RACP). When a client writes "Report Stored Records (all)", the server notifies one Glucose Measurement (sequence, base time, SFLOAT concentration) and then indicates the RACP response. Sends are queued, so onSent here is a deliberate choice: the RACP "operation complete" response is indicated only after the measurement send-result event; a notification has no ATT delivery confirmation.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `server.onWritten(...)` — Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
5. `ble.gattServer().notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.
6. `server.onSent(...)` — Use the asynchronous send result to inspect completion and failures. A notify result and an indication confirmation have different delivery guarantees.
7. `ble.gattServer().indicate(...)` — Queue an indication for subscribers. It has an ATT confirmation, but immediate acceptance still precedes completion and does not prove the peer application performed an action.
8. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
9. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

The RACP procedure is a sequence: subscribe, request stored records, receive measurement updates, then receive the Control Point indication. Keep the state machine and completion ordering when replacing the single demonstration record. A notification send result does not itself prove application-level receipt.

The server values are simulated protocol examples, not measurements from a medical sensor. Replace data acquisition separately while preserving the service encoding and validity checks on the client.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)

This minimal `onSent` handler advances by characteristic and state without branching on `result.success`. For a real record transfer, handle both immediate rejection and asynchronous send failures, and add state transitions that avoid reporting RACP success after a failed send.
