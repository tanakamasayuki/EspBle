# BatteryClient

> 日本語版: [README.ja.md](README.ja.md)

Central that scans for the standard Battery Service (`0x180F`), reads the Battery Level (`0x2A19`), then subscribes to its notifications.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Battery peripheral: the [BatteryServer](../BatteryServer/) example, or any device exposing the standard Battery Service

## What it does

- Active-scans and connects to the first advertiser offering `0x180F`
- On connect, reads the one-byte Battery Level and prints it
- After the read succeeds, subscribes to Battery Level notifications
- Prints each subsequent level change as a notification arrives

## Key APIs

- `ble.scanner().onResult(...)` / `advertisesService(...)` — pick a peer advertising the service
- `ble.readCharacteristic(...)` + `ble.onCharacteristicRead(...)` — read the current level
- `ble.subscribe(...)` + `ble.onSubscribed(...)` — enable notifications
- `ble.onNotification(...)` — receive level changes

## Expected Serial output

```
Battery: 75%
Battery subscription: ready
Battery changed: 76%
```
