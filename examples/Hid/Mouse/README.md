# Mouse

> 日本語版: [README.ja.md](README.ja.md)

A BLE HID mouse over GATT (HOGP) exposing a standard relative-motion pointer with buttons. Advertises the HID service `0x1812`; motion and clicks are triggered by Serial commands.

## Hardware

- 1 × ESP32-S3 running this sketch (HID device / peripheral)
- 1 × HID host: a PC or phone, or a second board running [KeyboardHost](../KeyboardHost/) (its `onMouse()` handler prints the events)

## What it does

- Calls `ble.hidMouse().configure()` before `begin()` to compose the HID service
- Enables security with bonding — HOGP requires an encrypted link
- Restarts advertising on each disconnect
- Send `m` to move the pointer (+12, -8), `c` to click the left button

## Key APIs

- `ble.hidMouse().configure()` — compose the HID mouse service before `begin()`
- `mouse.move(dx, dy)` — send a relative-motion report
- `mouse.click(ESP_BLE_HID_MOUSE_LEFT)` — press and release a button

## Expected Serial output

```
Send 'm' to move, 'c' to click.
```
