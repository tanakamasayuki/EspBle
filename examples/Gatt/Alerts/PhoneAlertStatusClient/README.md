# PhoneAlertStatusClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to a Phone Alert Status Service (0x180E), reads Alert Status, subscribes to Ringer Setting, and drives the Ringer Control Point (**Write Without Response**) to set Silent Mode then cancel it, printing the notified Ringer Setting each time.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Phone Alert Status peripheral: the [PhoneAlertStatusServer](../PhoneAlertStatusServer/) example

## What it does

- Scans for and connects to a device advertising 0x180E
- Reads Alert Status (0x2A3F) and prints the bitmask
- Subscribes to Ringer Setting (0x2A41) **notifications**
- Every 5 seconds, toggles the Ringer Control Point (0x2A40) between Set Silent Mode (1) and Cancel Silent Mode (3), and prints each notified Ringer Setting

## Key APIs

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, false)` — Write Without Response

## Expected Serial output

```
Alert Status: 0x01
Ringer Setting: Silent
Ringer Setting: Normal
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [PhoneAlertStatusClient.ino](PhoneAlertStatusClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [PhoneAlertStatusServer](../PhoneAlertStatusServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

PhoneAlertStatusClient - connect to a Phone Alert Status Service (0x180E), read Alert Status, subscribe to Ringer Setting, and drive the Ringer Control Point (Write Without Response) to set Silent Mode then Cancel it, printing the notified Ringer Setting each time.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.writeCharacteristic(...)` — Queue a remote write and inspect its completion event. Write Without Response provides no ATT response proving the peer application acted on the data.
2. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
3. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
4. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.
5. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
6. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
7. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
8. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
9. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
10. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Alert Status, Ringer Setting and Ringer Control Point are separate values. A control write changes the demonstration state; it does not connect this board to a phone call service.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
