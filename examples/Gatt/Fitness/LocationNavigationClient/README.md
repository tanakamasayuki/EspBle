# LocationNavigationClient

> 日本語版: [README.ja.md](README.ja.md)

Connects to a Location and Navigation Service (0x1819), reads LN Feature, subscribes to Location and Speed notifications, and decodes Instantaneous Speed (1/100 m/s) and the Location latitude/longitude (1e-7 deg).

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Location and Navigation peripheral: the [LocationNavigationServer](../LocationNavigationServer/) example or a commercial LN sensor

## What it does

- Scans for and connects to a device advertising 0x1819
- Reads LN Feature (0x2A6A)
- Subscribes to Location and Speed (0x2A67) **notifications**
- Decodes the flags + Instantaneous Speed + Location latitude/longitude and prints them

## Key APIs

- `ble.subscribe(connectionId, service, characteristic)` — subscribe to notifications

## Expected Serial output

```
Speed: 5.00 m/s, location: 35.681200, 139.767100
Speed: 5.10 m/s, location: 35.681200, 139.767100
```
