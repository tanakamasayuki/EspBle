# EnvironmentalClient

> 日本語版: [README.ja.md](README.ja.md)

Central / GATT client for the Environmental Sensing Service (0x181A). It sequentially reads and decodes Temperature (0x2A6E), Humidity (0x2A6F), and Pressure (0x2A6D), then subscribes to Temperature notifications.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × peripheral: the [EnvironmentalServer](../EnvironmentalServer/) example

## What it does

- Actively scans for a peer advertising 0x181A and connects
- Reads the three characteristics in order (temperature, humidity, pressure), decoding the little-endian values (0.01 °C, 0.01 %, 0.1 Pa)
- After the third read, subscribes to Temperature and prints each subsequent change notification

## Key APIs

- `ble.readCharacteristic(...)` — chained reads driven by `onCharacteristicRead`
- `ble.subscribe(...)` / `ble.onSubscribed(...)` — enable and confirm Temperature notifications
- `ble.onNotification(callback)` — decode changed Temperature values

## Notes

- Temperature and humidity are signed/unsigned 16-bit; pressure is a 32-bit value. Each decode checks the received length before use.

## Expected Serial output

```
Temperature raw: 2150 (0.01 C units)
Humidity raw: 4875 (0.01 % units)
Pressure raw: 1013250 (0.1 Pa units)
Temperature subscription: ready
Temperature changed raw: 2175 (0.01 C units)
```
