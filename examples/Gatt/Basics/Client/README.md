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
