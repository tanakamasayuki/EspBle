# HidKeyboard (Classic)

> 日本語版: [README.ja.md](README.ja.md)
> Choosing between the two radios: [BLE and Classic](../../../docs/CLASSIC_VS_BLE.md)

A Bluetooth Classic (BR/EDR) keyboard and mouse. The calls are the same ones
[Hid/KeyboardDevice](../../Hid/KeyboardDevice/) uses over BLE, because the
reports and the descriptors are shared; only the radio differs. **Classic works
on the original ESP32 only.**

## Hardware

- 1 × original ESP32 running this sketch (HID device)
- 1 × Classic HID host: a PC, or a second original ESP32 running
  [HidKeyboardHost](../HidKeyboardHost/)

## What it does

- Configures the keyboard and mouse profiles before `begin()`, because the
  composed Report Descriptor is part of the device record a Host reads while
  pairing
- Prints the LED state a Host sends back
- Types, taps single keys and moves the pointer from Serial commands

## Key APIs

- `bluetooth.hidKeyboard().configure()` / `bluetooth.hidMouse().configure()`
- `write("...")` / `tapKey(char)` / `pressUsage(usage)` / `releaseAll()`
- `onOutputReport()` — the Host's Caps Lock and Num Lock state

## Related guides

- [Classic guide §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.md#6-hid) — the SDP record, the 214-byte budget and what a Host decodes
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — report IDs on Classic, and how to verify a descriptor
- [BLE or Bluetooth Classic](../../../docs/CLASSIC_VS_BLE.md) — how Classic HID differs from BLE HID
