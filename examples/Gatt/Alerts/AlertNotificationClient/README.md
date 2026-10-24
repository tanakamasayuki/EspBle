# AlertNotificationClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to an Alert Notification Service (0x1811), reads Supported New Alert Category, subscribes to New Alert, and writes the Control Point "Notify New Alert Immediately" command; then decodes the New Alert (category, count, text) it triggers.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Alert Notification peripheral: the [AlertNotificationServer](../AlertNotificationServer/) example

## What it does

- Scans for and connects to a device advertising 0x1811
- Reads Supported New Alert Category (0x2A47) and prints the bitmask
- Subscribes to New Alert (0x2A46) **notifications**
- Writes the Alert Notification Control Point (0x2A44): "Notify New Alert Immediately" (command 2) for Email (category 1)
- Decodes the resulting New Alert (Category ID + count + text)

## Key APIs

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, true)` — Control Point write with response

## Expected Serial output

```
Supported categories: 0x0022
New Alert: category 1, count 3, text "Bob"
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [AlertNotificationClient.ino](AlertNotificationClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [AlertNotificationServer](../AlertNotificationServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

AlertNotificationClient - connect to an Alert Notification Service (0x1811), read Supported New Alert Category, subscribe to New Alert, and write the Control Point "Notify New Alert Immediately" command; then decode the New Alert (category, count, text) it triggers.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.
4. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
5. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
6. `ble.onSubscribed(...)` — Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
7. `ble.writeCharacteristic(...)` — Queue a remote write and inspect its completion event. Write Without Response provides no ATT response proving the peer application acted on the data.
8. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
9. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
10. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Supported-category bitmasks, Control Point commands and New Alert payloads serve different purposes. Keep the category ID valid and preserve the subscribe-before-request order.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
