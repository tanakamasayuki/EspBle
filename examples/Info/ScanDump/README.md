# ScanDump

> 日本語版: [README.ja.md](README.ja.md)

Diagnostic scanner that dumps every field EspBle extracts from each advertisement: address and address type, RSSI, connectable/scannable flags, name, every advertised service UUID, and the manufacturer data as hex. Use it to see what a peripheral actually advertises before writing a scan filter, or to debug why `advertisesService()` does not match.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- Any nearby BLE devices to inspect

## What it does

- Runs a continuous active scan (scan responses included, so more devices show their name)
- Prints one line per advertisement with all extracted fields
- Send `q` to print the diagnostic counters (`droppedScanResults` / `droppedEvents`)

## Key APIs

- `EspBleScanResult` — `address`, `addressType`, `rssi`, `connectable`, `scannable`, `name`, `serviceUuids[]` / `serviceUuidCount`, `manufacturerData`
- `scanResult.hasName()` / `hasManufacturerData()`
- `ble.scanner().droppedResultCount()` — scan results lost to queue overflow
- `ble.droppedEventCount()` — connection events lost to queue overflow

## Expected Serial output

```
Scanning. Send 'q' to print diagnostic counters.
5a:b8:1e:0c:2f:71 type=0 rssi=-52 connectable name="EspBle Keyboard" uuid=1812 uuid=180f
70:04:1d:32:99:a0 type=1 rssi=-78 connectable manufacturer[8]=4c0010050b1c72a1
counters: droppedScanResults=0 droppedEvents=0
```
