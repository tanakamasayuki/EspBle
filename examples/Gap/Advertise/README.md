# Advertise

> 日本語版: [README.ja.md](README.ja.md)

Starts connectable legacy advertising carrying a device name and a 16-bit service UUID (HID, `1812`). A minimal peripheral example; observe it with a generic BLE scanner app or with the paired [Scan](../Scan/) example on a second board.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- A BLE scanner — the [Scan](../Scan/) example on a second board, or a scanner app such as nRF Connect

## What it does

- Initializes the stack with the device name `EspBle Advertiser`
- Advertises the local name and the HID service UUID `1812`, merged into a single Complete List AD structure per UUID size (CSS Part A 1.1)
- Keeps advertising until reset; a central's connection attempt is accepted by the stack

## Key APIs

- `ble.begin(config)` — initialize the stack; `config.deviceName` sets the GAP device name
- `ble.advertising().setName(name)` — put the local name into the advertising payload
- `ble.advertising().addServiceUuid(uuid)` — advertise a service UUID (grouped by size into a single Complete List)
- `ble.advertising().start()` — start connectable legacy advertising; fails with `InvalidArgument` if the payload would exceed 31 bytes
- `ble.lastErrorName()` / `ble.lastErrorDetail()` — reason for a rejected request

## Expected Serial output

Silent on success. On failure:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```
