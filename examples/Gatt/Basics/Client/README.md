# Client

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to the [Gatt/Server](../Server/) example and walks through the central GATT client flow: database enumeration → known-UUID discovery → read → writes with and without response → descriptor read/write → reading a value the server builds on demand. Each request returns `bool` immediately and completes later as an event from `ble.update()`.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × ESP32-S3 running the [Gatt/Server](../Server/) example

## What it does

- Scans for the server's service UUID and connects
- Enumerates services, characteristics, and descriptors into a connection-scoped snapshot
- Discovers the known characteristic, then chains read, acknowledged/unacknowledged writes, and descriptor read/write
- Finally reads `10da4dd3-…`, whose value the server produces in its `onRead()` callback, and prints it as `Live:`
- Demonstrates that only one central GATT operation runs at a time — the next operation is issued from the completion callback of the previous one

## Writing it as a chain

Every GATT operation is asynchronous, and **a central runs only one at a time**. Requesting a second while one is in flight fails synchronously with `InvalidState`. So the procedure cannot be written top to bottom; it becomes a **chain: request, then issue the next one from the completion event**.

This example is that chain, made visible:

```
onConnected        → discoverServices()
onServicesDiscovered → discoverCharacteristic()
onCharacteristicDiscovered → readCharacteristic()
onCharacteristicRead → writeCharacteristic()
onCharacteristicWritten → (unacknowledged write) → readDescriptor()
onDescriptorRead   → writeDescriptor()
onDescriptorWritten → discoverCharacteristic(live) → readCharacteristic(live)
```

**There is one event per kind of operation, not per target.** With several characteristics in play, results arrive at the same callback in turn, so identify the target with `result.characteristicUuid` — or by handle when the UUID cannot tell them apart. This example reads two characteristics, so `onCharacteristicRead` branches.

Enumeration results are held as a **per-connection snapshot**, valid until the connection drops or the next enumeration. `discoveredService*()` and friends query that snapshot without touching the radio.

## Key APIs

- `ble.discoverServices()` / `onServicesDiscovered()` — enumerate the peer database
- `discoveredService*()` / `discoveredCharacteristic*()` / `discoveredDescriptor*()` — inspect the snapshot until disconnect or the next enumeration
- `ble.discoverCharacteristic(connectionId, serviceUuid, characteristicUuid)` — known-UUID discovery
- `ble.onCharacteristicDiscovered(callback)` — `EspBleGattResult` with `success`, properties, and `detail`
- `ble.readCharacteristic(...)` / `ble.onCharacteristicRead(callback)` — `result.value` holds the value (binary-safe)
- `ble.writeCharacteristic(connectionId, serviceUuid, characteristicUuid, value, withResponse)` / `ble.onCharacteristicWritten(callback)`
- `ble.readDescriptor()` / `writeDescriptor()` and their completion callbacks
- Trailing `timeoutMilliseconds` on each operation (default 10000; zero is invalid) — expiration completes with `EspBleError::Timeout`
- Central GATT operations are exclusive: a second request while one is in flight fails synchronously with `InvalidState`

## Notes

- **A value larger than the MTU is read in pieces automatically.** When it does not fit one ATT response, the client asks for the rest and joins it up (Read Long). `result.value` holds the whole thing, so nothing has to be reassembled by the application. Without this a long value would silently arrive truncated, which is why EspBle always reads this way.
- **Writes are not split.** A write goes out as a single ATT request; Long Write (writing across several requests) is not performed. The asymmetry with reads is because whether splitting works also depends on the peer's implementation. The limit for one request is MTU − 3 bytes, the same value as `maximumNotificationPayload()`.

## Expected Serial output

The enumeration counts depend on the peer's GATT database (including the standard services the backend provides), and `Live:` is `millis()` at the moment of the read.

```
Services: ..., characteristics: ..., descriptors: ...
Read: ready
Descriptor: EspBle value
Descriptor write complete
Live: 8421
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [Client.ino](Client.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [Server](../Server/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

Client - connect to the Gatt/Server example and run the central GATT client flow: database discovery -> known-UUID discovery -> read -> write -> descriptor access. Each step is a request API; completion arrives as an event from update(). The next operation is chained from the previous one's completion callback (central GATT operations are one-at-a-time).

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.discoverServices(...)` — Start remote database discovery; consume the results in its completion event instead of assuming the request immediately populated handles.
4. `ble.onServicesDiscovered(...)` — Inspect discovery success and the returned database before using discovered service, characteristic or descriptor handles.
5. `ble.discoverCharacteristic(...)` — Find the requested remote characteristic and use the corresponding discovery completion to continue reading or writing.
6. `ble.onCharacteristicDiscovered(...)` — Continue only if discovery succeeded; a connection alone does not guarantee that the requested UUID exists.
7. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.
8. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
9. `ble.writeCharacteristic(...)` — Queue a remote write and inspect its completion event. Write Without Response provides no ATT response proving the peer application acted on the data.
10. `ble.onCharacteristicWritten(...)` — Check result.success for the write outcome at the selected write mode; use a separate application response if you need confirmation of a command action.

## Verify and adapt

The generic GATT client shows discovery before characteristic and descriptor operations. A descriptor belongs to a particular characteristic; keep returned handles paired with their connection and owning value.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
