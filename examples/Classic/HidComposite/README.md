# HidComposite (Classic)

> 日本語版: [README.ja.md](README.ja.md)

One Bluetooth Classic (BR/EDR) HID device that is a keyboard, a mouse and media
keys at once. Classic registers a single device record, so all of it goes into
one composed Report Descriptor and each profile keeps its own report ID.

**How many profiles fit is limited here in a way it is not on BLE.** The record
has to fit a 300-byte SDP pad shared with the device strings; these three profiles
come to 144 descriptor bytes, and adding the gamepad makes 212, which does not
register. `begin()` refuses such a combination rather than starting a device no
Host can reach. For a gamepad, use [HidGamepad](../HidGamepad/) on its own. Mirrors [Hid/CompositeKeyboardMouse](../../Hid/CompositeKeyboardMouse/) on
the BLE side. **Classic works on the original ESP32 only.**

## Hardware

- 1 × original ESP32 running this sketch
- 1 × Classic HID host: a PC, or a second original ESP32 running
  [HidKeyboardHost](../HidKeyboardHost/)

## What it does

- Configures three profiles before `begin()`; what is configured decides the
  descriptor, and a profile added later would not be in the record the Host
  already read
- Picks one Class of Device, because a composite device still has to choose the
  one a Host will present it as
- Sends one report per profile from Serial commands

## Key APIs

- `hidKeyboard()` / `hidMouse()` / `hidConsumerControl()` — configured together,
  sent separately
- Each send goes out under its own report ID, so a Host that only understands the
  keyboard ignores the rest

## Serial commands

| Key | Effect |
|---|---|
| `k` | type "hi" |
| `m` | move the pointer |
| `v` | volume up |
| `r` | release the keyboard and mouse |
