# HidKeyboardNkro (Classic)

> 日本語版: [README.ja.md](README.ja.md)

A Bluetooth Classic (BR/EDR) keyboard with N-key rollover: every key is a bit in
the report, so there is no six-key limit. Mirrors
[Hid/KeyboardNkro](../../Hid/KeyboardNkro/) on the BLE side. **Classic works on
the original ESP32 only.**

## Hardware

- 1 × original ESP32 running this sketch
- 1 × Classic HID host: a PC, or a second original ESP32 running
  [HidKeyboardHost](../HidKeyboardHost/)

## What it does

- Calls `enableNkro(true)` **before** `configure()`, because NKRO changes the
  Report Descriptor and a Host reads that once, while pairing
- Holds eight keys at once, which a six-key report cannot express
- Types with the same convenience calls a 6KRO keyboard uses

## Key APIs

- `bluetooth.hidKeyboard().enableNkro(true)` — before `configure()`
- `pressUsage(usage)` — adds to the held state; the whole state travels in one
  report
- `write("...")` — unchanged by the descriptor choice

## Serial commands

| Key | Effect |
|---|---|
| `8` | hold eight keys at once |
| `w` | type "nkro" |
| `r` | release everything |
