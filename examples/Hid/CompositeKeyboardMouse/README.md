# CompositeKeyboardMouse

> 日本語版: [README.ja.md](README.ja.md)

A composite BLE HID device over GATT (HOGP) that is both a keyboard and a mouse. Configuring both profiles before `begin()` composes a single HID service (`0x1812`) with fixed Report IDs 1 (keyboard) and 2 (mouse). Input is triggered by Serial commands.

## Hardware

- 1 × ESP32-S3 running this sketch (HID device / peripheral)
- 1 × HID host: a PC or phone, or a second board running [KeyboardHost](../KeyboardHost/)

## What it does

- Calls `ble.hidKeyboard().configure()` and `ble.hidMouse().configure()` before `begin()`, composing one HID service with Report IDs 1 and 2
- Enables security with bonding — HOGP requires an encrypted link
- Restarts advertising on each disconnect
- Send `h` to type "hello", `m` to move the pointer (+10, +10)

## Key APIs

- `ble.hidKeyboard().configure()` / `ble.hidMouse().configure()` — both before `begin()` to build the composite service
- `keyboard.write("hello")` — type an ASCII string
- `mouse.move(dx, dy)` — send a relative-motion report

## Expected Serial output

```
Send 'h' to type hello, 'm' to move.
```
