# KeyboardDevice

> 日本語版: [README.ja.md](README.ja.md)

Turns the board into a BLE HID keyboard (HID over GATT, report protocol, fixed 6-key rollover). Pair it from a PC or smartphone and it types like a real keyboard; keystrokes are triggered by Serial commands. Also receives the host's LED output report (Num/Caps/Scroll Lock).

## Hardware

- 1 × ESP32-S3 running this sketch (HID device / peripheral)
- 1 × HID host: PC, smartphone, or a second board running the [KeyboardHost](../KeyboardHost/) example

## What it does

- Configures the HID Keyboard Device profile before `begin()` (HID + Device Information + Battery services are composed automatically, and the HID UUID/keyboard appearance are added to advertising)
- Enables security with bonding — HOGP requires an encrypted link, so the HID service attributes get encrypted permissions and input reports are only sent over encrypted, subscribed connections
- Sends Shift+A on `a` and releases all keys on `r`
- Prints LED state changes written by the host
- Restarts advertising after each disconnect

## Serial commands

| Command | Action |
|---------|--------|
| `a` | Press Shift+A (send an input report) |
| `r` | Release all keys |

## Key APIs

- `ble.hidKeyboardDevice().configure(config)` — must be called before `begin()`; `EspBleHidKeyboardDeviceConfig` covers manufacturer, PnP ID, country code, report ID, and initial battery level
- `EspBleHidKeyboardInputReport` — `modifiers` bitmask (e.g. `LeftShift`) and up to 6 HID usages in `keys[]`
- `keyboard.sendInputReport(report)` / `keyboard.releaseAll()`
- `keyboard.onOutputReport(callback)` — LED state from the host (`numLock()`, `capsLock()`, `scrollLock()`)
- `keyboard.setBatteryLevel(0..100)` — update the Battery Service level

## Expected Serial output

```
Send 'a' to type Shift+A and 'r' to release all keys.
Keyboard LEDs: num=0 caps=1 scroll=0
```
