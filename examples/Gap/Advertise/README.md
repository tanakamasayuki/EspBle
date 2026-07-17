# Advertise

> 日本語版: [README.ja.md](README.ja.md)

Starts connectable legacy advertising with a device name and a 16-bit service UUID. Scan for it with a generic BLE scanner app (e.g. nRF Connect) or with the [Scan](../Scan/) example on a second board.

## Hardware

- 1 × ESP32-S3 (or another board supported by EspBle)
- A BLE scanner (smartphone app or a second board running the Scan example)

## What it does

- Initializes the stack with the device name `EspBle Advertiser`
- Advertises the local name and the HID service UUID `1812` as one Complete List AD structure
- Keeps advertising until the board is reset (a connection attempt from a central is accepted by the stack)

## Key APIs

- `ble.begin(config)` — initialize the stack; `config.deviceName` sets the GAP device name
- `ble.advertising().setName(name)` — put the local name into the advertising payload
- `ble.advertising().addServiceUuid(uuid)` — advertise a 16/32/128-bit service UUID (grouped by size into a single Complete List)
- `ble.advertising().start()` — start connectable legacy advertising; fails with `InvalidArgument` if the payload would exceed 31 bytes
- `ble.lastErrorName()` / `ble.lastErrorDetail()` — reason for a rejected request

## Expected Serial output

Nothing is printed on success. On failure:

```
BLE init failed: InvalidState (...)
Advertising failed: InvalidArgument (...)
```
