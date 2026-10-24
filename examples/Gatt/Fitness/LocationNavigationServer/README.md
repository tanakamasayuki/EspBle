# LocationNavigationServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Location and Navigation Service (0x1819) peripheral. Location and Speed (0x2A67) is notified with a uint16 flags field plus the flags-selected data fields; LN Feature (0x2A6A) is a readable uint32.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [LocationNavigationClient](../LocationNavigationClient/) example, or any LN collector

## What it does

- Registers the Location and Navigation service and advertises 0x1819
- Publishes LN Feature = 0x00000005 (Instantaneous Speed + Location supported) as a readable value
- Every second, notifies a 12-byte Location and Speed measurement (flags 0x0005) with a speed drifting near 5 m/s plus a fixed Tokyo location

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — Location and Speed
- `ble.gattServer().setValue(...)` — seed the readable LN Feature
- `ble.gattServer().notify(...)` — push each measurement to subscribers

## Notes

- Fields follow flag-bit order: Instantaneous Speed (uint16, 1/100 m/s), then Location latitude and longitude (sint32, 1e-7 degrees).

## Expected Serial output

```
The server is silent; observe values on the client.
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [LocationNavigationServer.ino](LocationNavigationServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [LocationNavigationClient](../LocationNavigationClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

LocationNavigationServer - standard Location and Navigation Service (0x1819). Location and Speed (0x2A67) is notified with uint16 flags plus flags-selected fields; LN Feature (0x2A6A) is a readable uint32.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
5. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.
6. `ble.gattServer().notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.

## Verify and adapt

Latitude/longitude are signed scaled integers. Preserve sign and scale and inspect flags before reading position fields; the demonstration coordinates do not come from a GNSS receiver.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
