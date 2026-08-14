# HidGamepad (Classic)

> 日本語版: [README.ja.md](README.ja.md)
> Choosing between the two radios: [BLE and Classic](../../../docs/CLASSIC_VS_BLE.md)

A Bluetooth Classic (BR/EDR) HID gamepad. This is the example Classic exists
for: older game consoles and PCs accept Classic HID only, and BLE is not an
alternative for them. The calls are the same ones [Hid/Gamepad](../../Hid/Gamepad/)
uses over BLE — only the radio and the way a Host finds the device differ.

**Classic works on the original ESP32 only.** ESP32-S3/C3/C6/H2/P4 have no
BR/EDR radio.

## Hardware

- 1 × original ESP32 running this sketch (HID device)
- 1 × Classic HID host: a console, a PC, or a second original ESP32 running
  [HidKeyboardHost](../HidKeyboardHost/) (its raw report handler prints what
  arrives)

## What it does

- Configures the gamepad profile before `begin()`, because the composed Report
  Descriptor is part of the device record a Host reads while pairing
- Declares a Peripheral / gamepad Class of Device, which is what a Host uses to
  pick an icon and sometimes to decide whether to offer connecting
- Sends button, stick and hat reports from Serial commands

## Key APIs

- `bluetooth.hidGamepad().configure()` — add the gamepad to the composed HID
  device before `begin()`
- `send(x, y, z, rz, rx, ry, hat, buttons)` — one report from its axes
- `sendReport(report)` — the same thing from a filled `EspBleHidGamepadReport`
- `releaseAll()` — everything centred and released; a Host holds the last report
  it received until then

## Serial commands

| Key | Effect |
|---|---|
| `a` | button 1 |
| `b` | button 2 |
| `d` | left stick up-right, hat up, button 1 |
| `r` | release everything |
