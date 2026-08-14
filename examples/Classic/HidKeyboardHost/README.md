# HidKeyboardHost (Classic)

> 日本語版: [README.ja.md](README.ja.md)

A Bluetooth Classic (BR/EDR) HID host that receives decoded key and mouse events.
The callbacks and event types are the ones the BLE host uses
([Hid/KeyboardHost](../../Hid/KeyboardHost/)); only the radio and the way a peer
is named differ. **Classic works on the original ESP32 only.**

## Hardware

- 1 × original ESP32 running this sketch (HID host)
- 1 × Classic HID device: a keyboard, or a second original ESP32 running
  [HidKeyboard](../HidKeyboard/) or [HidGamepad](../HidGamepad/)

Put the device's address in `keyboardAddress`; [Inquiry](../Inquiry/) finds one.

## What it does

- Connects by address, because Classic has no advertisement to filter
- Decodes keyboard and mouse reports from the Report Descriptor the device sent
  over SDP, so a device with an unusual layout still arrives as events
- Delivers the keyboard state before the per-usage events of the same report, the
  same order the BLE host uses
- Passes anything it cannot classify to `onInputReport()` raw, with the report ID
  in front of the payload
- Writes the LEDs with a report ID taken from the peer's descriptor rather than
  an assumed one

## Key APIs

- `bluetooth.hidHost().connect(address)`
- `onKeyboardState()` / `onKeyboard()` / `onMouse()` / `onInputReport()`
- `setKeyboardLayout()` — this side's layout decides which character a usage
  stands for; the device chose the usage with its own layout
- `setKeyboardLeds(numLock, capsLock, scrollLock)`

## Serial commands

| Key | Effect |
|---|---|
| `c` | Caps Lock LED on |
| `0` | all LEDs off |

## Related guides

- [Classic guide §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.md#6-hid) — the SDP record, the 214-byte budget and what a Host decodes
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — report IDs on Classic, and how to verify a descriptor
- [BLE or Bluetooth Classic](../../../docs/CLASSIC_VS_BLE.md) — how Classic HID differs from BLE HID
