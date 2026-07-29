# Scan

> 日本語版: [README.ja.md](README.ja.md)

Runs a continuous active scan and prints every advertisement it receives: address, RSSI, and the device name when present. A **minimal** central example; pair it with the [Advertise](../Advertise/) example on a second board, or just observe nearby BLE devices.

Those three fields are all it prints. To see **every field** — service UUIDs, service data, manufacturer data, decoded iBeacons — use [Info/ScanDump](../../Info/ScanDump/) instead. This example stays focused on the smallest way to start a scan and receive results.

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
- `EspBleScanConfig` — `active`, `wantDuplicates`, `intervalMilliseconds`, `windowMilliseconds`, `durationSeconds`, `acceptListOnly`
  - With `acceptListOnly = true` only advertisers registered through `ble.addToAcceptList()` are reported. The controller drops the rest, so they never reach `onResult` ([Gap/AcceptList](../AcceptList/) uses the same list to restrict connections). Matching is by address, so a peer that rotates an RPA must be bonded first
- `ble.scanner().start(scanConfig)` / `ble.scanner().stop()`
- `ble.scanner().droppedResultCount()` — results dropped when the queue overflows

## Expected Serial output

```
5a:b8:1e:0c:2f:71 RSSI=-52 name=EspBle Advertiser
70:04:1d:32:99:a0 RSSI=-78
...
```
