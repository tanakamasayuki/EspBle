# AlertNotificationServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Alert Notification Service (0x1811) peripheral. Supported New Alert Category (0x2A47) is a readable uint16 bitmask, New Alert (0x2A46) is a **notification** carrying Category ID + count + text, and the Alert Notification Control Point (0x2A44) is writable.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [AlertNotificationClient](../AlertNotificationClient/) example, or any Alert Notification client

## What it does

- Registers the category bitmask, New Alert, and Control Point before `begin()` and advertises 0x1811
- Advertises support for Email (bit 1) and SMS/MMS (bit 5) categories (0x0022)
- On a "Notify New Alert Immediately" (command 2) Control Point write, notifies a New Alert for the requested category (count 3, text "Bob")

## Key APIs

- `ble.gattServer().onWritten(...)` — receive Control Point commands
- `ble.gattServer().notify(...)` — notify a New Alert

## Expected Serial output

```
Notify New Alert for category 1
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [AlertNotificationServer.ino](AlertNotificationServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [AlertNotificationClient](../AlertNotificationClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

AlertNotificationServer - standard Alert Notification Service (0x1811). Supported New Alert Category (0x2A47) is a readable uint16 bitmask; New Alert (0x2A46) is notified with Category ID + count + text; the Alert Notification Control Point (0x2A44) is writable. "Notify New Alert Immediately" (command 2) triggers a New Alert notification.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `server.onWritten(...)` — Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
5. `ble.gattServer().notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.
6. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
7. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

Supported-category bitmasks, Control Point commands and New Alert payloads serve different purposes. Keep the category ID valid and preserve the subscribe-before-request order.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
