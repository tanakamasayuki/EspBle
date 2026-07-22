# AlertNotificationServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Alert Notification Service (0x1811) peripheral. Supported New Alert Category (0x2A47) is a readable uint16 bitmask, New Alert (0x2A46) is a **notification** carrying Category ID + count + text, and the Alert Notification Control Point (0x2A44) is writable.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [AlertNotificationClient](../AlertNotificationClient/) example, or any Alert Notification client

## What it does

- Registers the category bitmask, New Alert, and Control Point before `begin()` and advertises 0x1811
- Advertises support for Email (bit 1) and SMS/MMS (bit 5) categories (0x0022)
- On a "Notify New Alert Immediately" (command 2) Control Point write, notifies a New Alert for the requested category (count 3, text "Bob")

## Key APIs

- `ble.gattServer().onWritten(...)` — receive Control Point commands
- `ble.gattServer().notify(...)` — notify a New Alert

## Expected Serial output

```
Notify New Alert for category 1
```
