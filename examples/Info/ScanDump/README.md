# ScanDump

> 日本語版: [README.ja.md](README.ja.md)

Diagnostic scanner that dumps every field EspBle extracts from each advertisement: address and address type, RSSI, connectable/scannable flags, name, every advertised service UUID, the service data, and the manufacturer data as hex. iBeacon payloads are decoded into UUID / major / minor / measured power. Use it to see what a peripheral actually advertises before writing a scan filter, or to debug why `advertisesService()` does not match.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- Any nearby BLE devices to inspect

## What it does

- Runs a continuous active scan (scan responses included, so more devices show their name)
- Prints one line per advertisement with all extracted fields
- Decodes manufacturer data that matches the iBeacon layout (Apple company ID `0x004C`)
- Send `q` to print the diagnostic counters (`droppedScanResults` / `droppedEvents`)

## Key APIs

- `EspBleScanResult` — `address`, `addressType`, `rssi`, `connectable`, `scannable`, `name`, `serviceUuids[]` / `serviceUuidCount`, `manufacturerData`, `serviceData` / `serviceDataUuid`, `appearance`, `txPowerLevel`
- `scanResult.hasName()` / `hasManufacturerData()` / `hasServiceData()` / `hasAppearance()` / `hasTxPowerLevel()`
- `espBleDecodeIBeacon()` from `EspBleIBeacon.h` — decode an iBeacon manufacturer-data payload
- `ble.scanner().droppedResultCount()` — scan results lost to queue overflow
- `ble.droppedEventCount()` — connection events lost to queue overflow

## Expected Serial output

```
Scanning. Send 'q' to print diagnostic counters.
5a:b8:1e:0c:2f:71 type=0 rssi=-52 connectable name="EspBle Keyboard" uuid=1812 uuid=180f
d0:cf:13:58:fd:95 type=0 rssi=-14 connectable scannable name="EspBle Scan Response" appearance=0x0341 txpower=9dBm loss=23dB uuid=5266f727-49d7-4eaf-a6f1-7363616e7270 manufacturer[5]=ffff010203
70:04:1d:32:99:a0 type=1 rssi=-78 connectable manufacturer[8]=4c0010050b1c72a1
d0:cf:13:58:fd:95 type=0 rssi=-13 uuid=0000181a-0000-1000-8000-00805f9b34fb servicedata[0000181a-0000-1000-8000-00805f9b34fb][2]=c409
d0:cf:13:58:fd:95 type=0 rssi=-13 manufacturer[25]=4c0002150102030405060708090a0b0c0d0e0f1000640001c5 ibeacon uuid=01020304-0506-0708-090a-0b0c0d0e0f10 major=100 minor=1 power=-59
counters: droppedScanResults=0 droppedEvents=0
```
