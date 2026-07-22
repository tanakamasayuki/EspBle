# KeyboardDevice

> 日本語版: [README.ja.md](README.ja.md)

Turns the board into a BLE HID keyboard over GATT (HOGP, fixed 6-key rollover) that advertises the HID service `0x1812`. Paired from a PC or phone it types like a real keyboard; keystrokes are triggered by Serial commands, and it also receives the host's LED output report. HID Boot Protocol is enabled so BIOS-class hosts are supported.

## Hardware

- 1 × ESP32-S3 running this sketch (HID device / peripheral)
- 1 × HID host: a PC or phone, or a second board running [KeyboardHost](../KeyboardHost/)

## What it does

- Configures the HID Keyboard profile before `begin()` (HID + Device Information + Battery services are composed automatically, and the HID UUID / keyboard appearance are added to advertising)
- Opts into Boot Protocol so `onProtocolMode()` is meaningful and boot hosts receive the 8-byte boot report automatically
- Enables security with bonding — HOGP requires an encrypted link
- Prints the host's LED state (Num/Caps/Scroll Lock) via `onOutputReport()`, and the current Protocol Mode via `onProtocolMode()`
- Restarts advertising on each disconnect
- Send `a` to press Shift+A, `r` to release all keys

## Key APIs

- `ble.hidKeyboard()` / `keyboard.configure(config)` — configure before `begin()`; `EspBleHidKeyboardConfig` sets `manufacturer` and opts into `bootProtocol`
- `EspBleHidKeyboardInputReport` — `modifiers` bitmask (e.g. `LeftShift`) plus up to 6 HID usages in `keys[]`
- `keyboard.sendReport(report)` / `keyboard.releaseAll()`
- `keyboard.onOutputReport(cb)` — LED state (`numLock()`, `capsLock()`, `scrollLock()`)
- `keyboard.onProtocolMode(cb)` — compare against `EspBleHidKeyboard::BootProtocolMode`

## Notes

- The report ID and reserved byte in the input report are handled internally.
- Output-report and protocol-mode events are delivered from `ble.update()`.

## Expected Serial output

```
Send 'a' to type Shift+A and 'r' to release all keys.
Protocol Mode: Report
Keyboard LEDs: num=0 caps=1 scroll=0
```
