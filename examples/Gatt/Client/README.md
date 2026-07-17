# Client

> 日本語版: [README.ja.md](README.ja.md)

Connects to the [Gatt/Server](../Server/) example and walks through the central GATT client flow: known-UUID discovery → read → write with response. Each step is requested with a `bool`-returning call and completes later as an event from `ble.update()`.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × ESP32-S3 running the [Gatt/Server](../Server/) example

## What it does

- Scans for the server's service UUID and connects
- Discovers the known characteristic, then chains: read → print the value → write `hello from Central` → print the write result
- Demonstrates that only one central GATT operation runs at a time — the next operation is issued from the completion callback of the previous one

## Key APIs

- `ble.discoverCharacteristic(connectionId, serviceUuid, characteristicUuid)` — known-UUID discovery (service/characteristic enumeration is not implemented yet)
- `ble.onCharacteristicDiscovered(callback)` — `EspBleGattResult` with `success`, properties, and `detail`
- `ble.readCharacteristic(...)` / `ble.onCharacteristicRead(callback)` — `result.value` holds the value (binary-safe)
- `ble.writeCharacteristic(connectionId, serviceUuid, characteristicUuid, value, withResponse)` / `ble.onCharacteristicWritten(callback)`
- Central GATT operations are exclusive: a second request while one is in flight fails synchronously with `InvalidState`

## Expected Serial output

```
Read: ready
Write complete
```
