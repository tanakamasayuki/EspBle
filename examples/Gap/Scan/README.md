# Scan

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 2, "GAP"

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

## Related guides

- [BLE guide §2 GAP](../../../docs/GUIDE_BLE_BASICS.md#2-gap--finding-and-connecting) — advertising, scanning and connecting

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [Scan.ino](Scan.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [Advertise](../Advertise/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

Scan - run a continuous active scan and print each advertisement's address, RSSI, and name. Minimal central example; pair with Gap/Advertise or observe nearby BLE devices.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.

## Verify and adapt

Use active scanning when the peer name is in scan response. Enable duplicates only if repeated advertisements are needed; absent names and repeated addresses are normal scan conditions.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
