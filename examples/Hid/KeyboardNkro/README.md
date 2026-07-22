# KeyboardNkro

> 日本語版: [README.ja.md](README.ja.md)

A BLE HID keyboard over GATT (HOGP) that uses the 29-byte NKRO (N-key rollover) bitmap input report instead of the fixed 6-key report, allowing any number of keys to be reported simultaneously. Advertises the HID service `0x1812`; keystrokes are triggered by Serial commands.

## Hardware

- 1 × ESP32-S3 running this sketch (HID device / peripheral)
- 1 × HID host: a PC or phone, or a second board running [KeyboardHost](../KeyboardHost/)

## What it does

- Calls `enableNkro()` before `configure()` to switch the keyboard to the 29-byte NKRO bitmap report
- Requests a preferred MTU of 64 (a 29-byte NKRO input report needs MTU ≥ 32)
- Enables security with bonding — HOGP requires an encrypted link
- Restarts advertising on each disconnect
- Send `n` to press eight simultaneous keys, `r` to release all

## Key APIs

- `ble.hidKeyboard().enableNkro()` — must be called before `configure()`
- `keyboard.configure()` — compose the HID service before `begin()`
- `keyboard.pressUsage(usage)` — set one HID usage down in the NKRO bitmap
- `keyboard.releaseAll()` — clear all keys
- `config.preferredMtu = 64` — negotiate an MTU large enough for the report

## Notes

- The NKRO bitmap report layout matches EspUsbDevice, so the same usages map identically.

## Expected Serial output

```
Send 'n' for eight simultaneous keys, 'r' to release all.
```
