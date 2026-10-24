# ImmediateAlertClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

The Find Me profile **locator** role. Connects to an Immediate Alert Service (0x1802) and writes Alert Level (0x2A06) with **Write Without Response** to make the target alert.

## Hardware

- 1 × ESP32-S3 running this sketch (central / Find Me locator)
- 1 × Immediate Alert peripheral: the [ImmediateAlertServer](../ImmediateAlertServer/) example

## What it does

- Scans for and connects to a device advertising 0x1802
- On connect, writes Alert Level = 2 (High Alert) with Write Without Response
- Five seconds later, writes Alert Level = 0 (No Alert)

## Key APIs

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, false)` — Write Without Response (last argument false)

## Expected Serial output

```
High Alert sent
No Alert sent
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [ImmediateAlertClient.ino](ImmediateAlertClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [ImmediateAlertServer](../ImmediateAlertServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

ImmediateAlertClient - the Find Me profile locator role. Connect to an Immediate Alert Service (0x1802) and write Alert Level (0x2A06) with Write Without Response to make the target alert. This example raises High Alert on connect and clears it after five seconds.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.writeCharacteristic(...)` — Queue a remote write and inspect its completion event. Write Without Response provides no ATT response proving the peer application acted on the data.
2. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
3. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
4. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
5. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
6. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Alert Level is a command value sent with Write Without Response. Receiving or logging the command does not drive a buzzer; add your output action separately and define how it is stopped.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
