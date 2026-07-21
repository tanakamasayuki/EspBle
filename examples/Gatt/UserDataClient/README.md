# UserDataClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a User Data Service (0x181C), subscribes to Database Change Increment notifications, reads Age, and writes a new First Name and Age. Each write bumps the increment, which arrives as a notification.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × User Data peripheral: the [UserDataServer](../UserDataServer/) example

## What it does

- Scans for and connects to a device advertising 0x181C
- Subscribes to Database Change Increment (0x2A99) **notifications**
- Reads Age (0x2A80), then writes First Name (0x2A8A) = "Ada" and Age = 42 **with response**
- Prints each Database Change Increment notification as the server bumps it

## Key APIs

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, true)` — write with response

## Expected Serial output

```
Age: 25
Database Change Increment: 1
Database Change Increment: 2
```
