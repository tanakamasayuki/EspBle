# EnvironmentalServer

> 日本語版: [README.ja.md](README.ja.md)

Standard Environmental Sensing Service (0x181A) peripheral. It publishes Temperature (0x2A6E), Humidity (0x2A6F), and Pressure (0x2A6D) using the standard little-endian integer scales: 0.01 °C, 0.01 %, and 0.1 Pa. Temperature is readable and notifiable; humidity and pressure are read-only.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [EnvironmentalClient](../EnvironmentalClient/) example, or any BLE central/phone

## What it does

- Registers the Environmental Sensing service and advertises 0x181A
- Seeds Temperature 21.50 °C, Humidity 48.75 %, and Pressure 101325.0 Pa as readable values
- Reads Serial: send `+` or `-` to change the temperature by 0.25 °C, then re-publishes and notifies the new value

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .readable = true, .notifiable = true })` — Temperature
- `ble.gattServer().setValue(...)` — update the stored characteristic value
- `ble.gattServer().notify(...)` — notify subscribers; the return flag reports whether it was accepted

## Notes

- `notify()` returns 0 when no client has subscribed to Temperature yet, and 1 once a subscriber is connected.

## Expected Serial output

```
Send '+' or '-' to change temperature by 0.25 C.
Temperature raw: 2175 (notification accepted: 1)
Temperature raw: 2200 (notification accepted: 1)
```
