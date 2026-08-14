# HidMouse (Classic)

> 日本語版: [README.ja.md](README.ja.md)
> Choosing between the two radios: [BLE and Classic](../../../docs/CLASSIC_VS_BLE.md)

A Bluetooth Classic (BR/EDR) HID mouse. The calls are the same ones
[Hid/Mouse](../../Hid/Mouse/) uses over BLE; only the radio differs. **Classic
works on the original ESP32 only.**

## Hardware

- 1 × original ESP32 running this sketch (HID device)
- 1 × Classic HID host: a PC, or a second original ESP32 running
  [HidKeyboardHost](../HidKeyboardHost/) (its `onMouse()` handler prints the
  decoded events)

## What it does

- Configures the mouse profile before `begin()`
- Declares a Peripheral / pointing-device Class of Device so a Host lists it as
  a mouse rather than as uncategorised
- Moves, clicks, scrolls and drags from Serial commands

## Key APIs

- `bluetooth.hidMouse().configure()` — add the mouse before `begin()`
- `move(dx, dy)` — relative, signed motion
- `click(button)` — press and release in one call
- `wheel(delta)` — scroll without moving the pointer or releasing buttons
- `press(button)` / `releaseAll()` — held buttons add up, which is what a drag
  needs; `buttons()` reads what is held

## Serial commands

| Key | Effect |
|---|---|
| `m` | move (+12, -8) |
| `c` | left click |
| `w` | scroll down one step |
| `p` | press left, then move — a drag |
| `r` | release every button |
