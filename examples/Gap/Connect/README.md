# Connect

> 日本語版: [README.ja.md](README.ja.md)

Scans for a peripheral advertising a specific service UUID and connects to it as a central. Demonstrates the asynchronous connection model: `connect()` only accepts the request, and completion (or failure) arrives later as an event delivered from `ble.update()`.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × BLE peripheral advertising the target service UUID — e.g. a second board running the [Gatt/Server](../../Gatt/Server/) example (change `TARGET_SERVICE_UUID` to match)

## What it does

- Starts an active scan and looks for `TARGET_SERVICE_UUID` in each result
- Stops the scan and requests a connection to the first match
- Prints connect, disconnect, and connection-failure events with the library connection ID
- Retries on the next scan result after a disconnect or failure

Edit `TARGET_SERVICE_UUID` at the top of the sketch to the UUID your peripheral advertises.

## Key APIs

- `scanResult.advertisesService(uuid)` — match either the 16-bit form (`"1812"`) or the 128-bit form
- `ble.connect(scanResult)` — accepts the request and returns immediately; the connection itself runs on an internal task
  - `ble.connect(scanResult, timeoutMilliseconds)` — the timeout is enforced from `update()` (default 10000 ms)
- `ble.onConnected(callback)` / `ble.onDisconnected(callback)` — both carry the same stable library `connection.id`
- `ble.onConnectionFailed(callback)` — asynchronous failure with `failure.detail`

## Expected Serial output

```
Connected to 5a:b8:1e:0c:2f:71 (id=1)
Disconnected (id=1)
```
