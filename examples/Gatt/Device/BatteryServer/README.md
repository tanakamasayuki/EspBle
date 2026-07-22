# BatteryServer

> 日本語版: [README.ja.md](README.ja.md)

Peripheral publishing the standard Battery Service (`0x180F`) with a readable and notifiable Battery Level characteristic (`0x2A19`).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [BatteryClient](../BatteryClient/) example, or any BLE central/phone

## What it does

- Registers the Battery Level characteristic (readable + notifiable) before `begin()` and advertises `0x180F`
- Starts at 75% and logs when a client turns notifications on/off
- Send `+` or `-` over Serial to change the level (clamped 0–100); each change updates the stored value and notifies subscribers

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .readable = true, .notifiable = true })` — declare the Battery Level characteristic
- `ble.gattServer().setValue(...)` — store the current level
- `ble.gattServer().notify(...)` — push the level to subscribers (returns whether it was accepted)
- `ble.gattServer().onSubscriptionChanged(...)` — observe when notifications are enabled/disabled

## Expected Serial output

```
Send '+' or '-' to change the Battery Level.
Battery notifications: 1
Battery: 76% (notification accepted: 1)
```
