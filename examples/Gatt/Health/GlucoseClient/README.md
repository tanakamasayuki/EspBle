# GlucoseClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to a Glucose Service (0x1808), subscribes to Glucose Measurement notifications and Record Access Control Point (RACP) indications, then writes "Report Stored Records (all)" and prints the decoded records and the RACP response.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Glucose peripheral: the [GlucoseServer](../GlucoseServer/) example or a commercial glucose meter

## What it does

- Scans for and connects to a device advertising 0x1808
- Subscribes to Glucose Measurement (0x2A18) notifications and RACP (0x2A52) indications
- Writes the RACP "Report Stored Records (all)" request (opcode 1, operator 1)
- Decodes each notified measurement (sequence number + SFLOAT concentration) and the RACP response

## Key APIs

- `ble.subscribe(..., true)` / `ble.subscribe(..., false)` — notifications for the measurement, indications for the RACP
- `ble.writeCharacteristic(..., true)` — reliable write of the RACP request

## Expected Serial output

```
Glucose record #1: 99
RACP response: request=1 status=1
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [GlucoseClient.ino](GlucoseClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [GlucoseServer](../GlucoseServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

GlucoseClient - connect to a Glucose Service (0x1808), subscribe to Glucose Measurement notifications and the Record Access Control Point (RACP) indications, then write "Report Stored Records (all)" and print the decoded measurements and the RACP response.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
4. `ble.onSubscribed(...)` — Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
5. `ble.writeCharacteristic(...)` — Queue a remote write and inspect its completion event. Write Without Response provides no ATT response proving the peer application acted on the data.
6. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
7. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
8. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

The RACP procedure is a sequence: subscribe, request stored records, receive measurement updates, then receive the Control Point indication. Keep the state machine and completion ordering when replacing the single demonstration record. A notification send result does not itself prove application-level receipt.

The server values are simulated protocol examples, not measurements from a medical sensor. Replace data acquisition separately while preserving the service encoding and validity checks on the client.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
