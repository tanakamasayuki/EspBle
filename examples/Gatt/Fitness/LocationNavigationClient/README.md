# LocationNavigationClient

> 日本語版: [README.ja.md](README.ja.md)

Central / GATT client for the Location and Navigation Service (0x1819). It reads LN Feature (0x2A6A), subscribes to Location and Speed (0x2A67) notifications, and decodes Instantaneous Speed and the Location latitude/longitude.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × peripheral: the [LocationNavigationServer](../LocationNavigationServer/) example

## What it does

- Scans for a connectable peer advertising 0x1819 and connects
- On connect, reads the 4-byte LN Feature, then subscribes to Location and Speed
- Decodes only measurements carrying both Instantaneous Speed (bit 0) and Location (bit 2): speed (1/100 m/s) and latitude/longitude (1e-7 degrees)

## Key APIs

- `ble.readCharacteristic(...)` — read LN Feature
- `ble.subscribe(...)` — enable Location and Speed notifications
- `ble.onNotification(callback)` — decode speed and location

## Expected Serial output

```
Speed: 5.10 m/s, location: 35.681200, 139.767100
Speed: 5.20 m/s, location: 35.681200, 139.767100
```
