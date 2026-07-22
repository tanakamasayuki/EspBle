# Client

> 日本語版: [README.ja.md](README.ja.md)

Connects to the [Gatt/Server](../Server/) example and walks through the central GATT client flow: database enumeration → known-UUID discovery → read → writes with and without response → descriptor read/write. Each request returns `bool` immediately and completes later as an event from `ble.update()`.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × ESP32-S3 running the [Gatt/Server](../Server/) example

## What it does

- Scans for the server's service UUID and connects
- Enumerates services, characteristics, and descriptors into a connection-scoped snapshot
- Discovers the known characteristic, then chains read, acknowledged/unacknowledged writes, and descriptor read/write
- Demonstrates that only one central GATT operation runs at a time — the next operation is issued from the completion callback of the previous one

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

## Expected Serial output

```
Read: ready
Descriptor: EspBle value
Descriptor write complete
```
