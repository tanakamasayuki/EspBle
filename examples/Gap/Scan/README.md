# Scan

> 日本語版: [README.ja.md](README.ja.md)

Runs a continuous active scan and prints every advertisement it receives: address, RSSI, and the device name when present. A minimal central example; pair it with the [Advertise](../Advertise/) example on a second board, or just observe nearby BLE devices.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- Optional peer — the [Advertise](../Advertise/) example on a second board, or any nearby BLE device

## What it does

- Starts an active scan with no duration limit (`durationSeconds = 0`)
- Delivers each result as a value-type copy from the `ble.update()` context — the callback never runs on the BLE stack task
- Prints address, RSSI, and name (when present) for every result

## Key APIs

- `ble.scanner().onResult(callback)` — receives an `EspBleScanResult` per advertisement
  - `scanResult.address`, `scanResult.rssi`, `scanResult.hasName()`, `scanResult.name`
  - also available: `advertisesService(uuid)`, `connectable`, manufacturer data
- `EspBleScanConfig` — `active`, `wantDuplicates`, `intervalMilliseconds`, `windowMilliseconds`, `durationSeconds`
- `ble.scanner().start(scanConfig)` / `ble.scanner().stop()`
- `ble.scanner().droppedResultCount()` — results dropped when the queue overflows

## Expected Serial output

```
5a:b8:1e:0c:2f:71 RSSI=-52 name=EspBle Advertiser
70:04:1d:32:99:a0 RSSI=-78
...
```
