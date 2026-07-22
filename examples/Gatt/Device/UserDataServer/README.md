# UserDataServer

> 日本語版: [README.ja.md](README.ja.md)

Standard User Data Service (0x181C) peripheral. Age (0x2A80) is a read/write uint8, First Name (0x2A8A) is a read/write utf8s, and Database Change Increment (0x2A99) is a read/write/**notify** uint32. Each write to Age or First Name bumps the increment and notifies it.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [UserDataClient](../UserDataClient/) example, or any User Data collector

## What it does

- Registers Age, First Name, and Database Change Increment before `begin()` and advertises 0x181C
- Handles client writes in `onWritten`: stores the new Age, logs the First Name, and bumps + notifies the Database Change Increment
- Starts with Age 25 and increment 0

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .readable = true, .writable = true })` — writable characteristics
- `ble.gattServer().onWritten(...)` — receive client writes in loop context
- `ble.gattServer().notify(...)` — notify the updated Database Change Increment

## Expected Serial output

```
First Name updated: Ada
Age updated: 42
```
