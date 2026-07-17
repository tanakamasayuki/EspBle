# KeyboardHost

> 日本語版: [README.ja.md](README.ja.md)

Connects to a BLE keyboard as a HID host: scans for the HID service, pairs, discovers the keyboard, and prints key events. Works with commercial BLE keyboards and with the [KeyboardDevice](../KeyboardDevice/) example on a second board. Also demonstrates writing the keyboard LEDs.

## Hardware

- 1 × ESP32-S3 running this sketch (HID host / central)
- 1 × BLE keyboard (commercial, or a second board running KeyboardDevice)

## What it does

- Scans for devices advertising the HID service `1812` and connects to the first one
- Enables security with bonding, and starts HID discovery **after** `onSecurityChanged` reports success — commercial keyboards require an encrypted link for their HID attributes
- Prints the discovery result (report ID, battery level), raw usage state changes, and per-key press events with the layout-converted ASCII value
- Sets the keyboard layout to EN-US for the `onKeyboard()` conversion (19 EspUsbHost-compatible layouts are available)
- Writes the Caps Lock LED on `c` and clears the LEDs on `0`

## Serial commands

| Command | Action |
|---------|--------|
| `c` | Set the Caps Lock LED on the keyboard |
| `0` | Clear all keyboard LEDs |

## Key APIs

- `ble.hidKeyboardHost().discover(connectionId)` — explicit HID discovery; call it again with the new connection ID after every reconnect
- `keyboard.onDiscovered(callback)` — `EspBleHidKeyboardHostDiscovery` with `success`, `reportId`, battery info, and `detail`
- `keyboard.onKeyboardState(callback)` — layout-independent 256-bit usage snapshot (`isDown()`, `wasPressed()`, `wasReleased()`)
- `keyboard.setKeyboardLayout(layout)` / `keyboard.onKeyboard(callback)` — per-usage press/release events with `unicode` and `ascii` (ISO-8859-1) conversion
- `keyboard.setKeyboardLeds(connectionId, num, caps, scroll)` — fire-and-forget LED write (Write Without Response)

## Expected Serial output

```
Keyboard ready: report=1 battery=73%
Key pressed: usage=0x04 ascii=0x61
Keyboard state: modifiers=0x02 A=1 pressed=1 released=0
```
