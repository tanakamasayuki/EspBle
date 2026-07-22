# CurrentTimeClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Current Time Service (0x1805), reads and decodes the 10-byte Current Time (0x2A2B), then subscribes to notifications and decodes each update.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Current Time peripheral: the [CurrentTimeServer](../CurrentTimeServer/) example

## What it does

- Active-scans for and connects to a device advertising 0x1805
- Reads Current Time (0x2A2B) and decodes the 10-byte wire format (year, date, time, weekday, fraction, adjust reason)
- Subscribes to Current Time notifications and decodes each change

## Key APIs

- `ble.readCharacteristic(...)` — read the initial Current Time
- `ble.subscribe(...)` — subscribe to Current Time notifications
- `ble.onNotification(...)` — decode each notified update

## Expected Serial output

```
Current Time: 2026-07-19 12:34:56 weekday=7 fraction=0/256 reason=0x01
Current Time subscription: ready
Current Time changed: 2026-07-19 12:34:57 weekday=7 fraction=0/256 reason=0x01
```
