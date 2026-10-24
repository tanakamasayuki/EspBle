# NusClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Central that talks to a Nordic UART Service (NUS) server by composing the generic GATT client API: it subscribes to TX notifications (`6e400003-…`) and sends each Serial line to RX (`6e400002-…`) using Write Without Response, under service `6e400001-…`.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × ESP32-S3 running the [NusServer](../NusServer/) example (or any NUS-compatible peripheral)

## What it does

- Scans for the NUS service UUID and connects
- Subscribes to TX notifications after connecting and prints `NUS ready`
- Reads each non-empty line from Serial and writes it to RX (Write Without Response)
- Prints TX notifications received back from the server as `RX: …`

## Key APIs

- `ble.subscribe(connectionId, serviceUuid, characteristicUuid)` — subscribe to TX notifications
- `ble.onSubscribed(callback)` — subscription completion (`result.success`)
- `ble.writeCharacteristic(connectionId, serviceUuid, characteristicUuid, value, false)` — Write Without Response to RX
- `ble.onCharacteristicWritten(callback)` — write acceptance result
- `ble.onNotification(callback)` — TX payload, filtered by `characteristicUuid.equalsIgnoreCase(...)`

## Notes

- This shows NUS built purely from the generic GATT client API; no stream semantics or automatic packet framing are implied. Type a line in the Serial monitor to send it.

## Expected Serial output

```
NUS ready: 1
TX accepted: 1
RX: hello
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [NusClient.ino](NusClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [NusServer](../NusServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
4. `ble.onSubscribed(...)` — Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
5. `ble.onCharacteristicWritten(...)` — Check result.success for the write outcome at the selected write mode; use a separate application response if you need confirmation of a command action.
6. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
7. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
8. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
9. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.
10. `ble.writeCharacteristic(...)` — Queue a remote write and inspect its completion event. Write Without Response provides no ATT response proving the peer application acted on the data.

## Verify and adapt

NUS is a pair of characteristics, not an automatic Serial stream. This example uses lines from Serial; design framing and chunking explicitly for binary or long messages, and treat subscription success as readiness.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
