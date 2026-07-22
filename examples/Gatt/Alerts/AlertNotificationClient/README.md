# AlertNotificationClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to an Alert Notification Service (0x1811), reads Supported New Alert Category, subscribes to New Alert, and writes the Control Point "Notify New Alert Immediately" command; then decodes the New Alert (category, count, text) it triggers.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Alert Notification peripheral: the [AlertNotificationServer](../AlertNotificationServer/) example

## What it does

- Scans for and connects to a device advertising 0x1811
- Reads Supported New Alert Category (0x2A47) and prints the bitmask
- Subscribes to New Alert (0x2A46) **notifications**
- Writes the Alert Notification Control Point (0x2A44): "Notify New Alert Immediately" (command 2) for Email (category 1)
- Decodes the resulting New Alert (Category ID + count + text)

## Key APIs

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, true)` — Control Point write with response

## Expected Serial output

```
Supported categories: 0x0022
New Alert: category 1, count 3, text "Bob"
```
