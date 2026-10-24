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
- Detects a button press pulling GPIO 0 to GND and, when ready, types `hello` and moves the mouse relatively; `INPUT_PULLUP` keeps an unpressed input HIGH

## Key APIs

- `bluetooth.hidKeyboard().configure()` / `bluetooth.hidMouse().configure()`
- `ready()` — check whether the host can receive input reports
- `write("hello")` / `bluetooth.hidMouse().move(10, 0)` — send text and relative motion on the button press edge
- `onOutputReport()` — the Host's Caps Lock and Num Lock state

## Related guides

- [Classic guide §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.md#6-hid) — the SDP record, the 214-byte budget and what a Host decodes
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — report IDs on Classic, and how to verify a descriptor
- [BLE or Bluetooth Classic](../../../docs/CLASSIC_VS_BLE.md) — how Classic HID differs from BLE HID

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HidKeyboard.ino](HidKeyboard.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [HidKeyboardHost](../HidKeyboardHost/README.md). For a two-board test, prepare the matching Classic profile on the peer; make the accepting device discoverable when needed and configure the initiating side with its address.

## How this example is structured

A Classic HID keyboard and mouse. The calls are the same ones the BLE examples use (examples/Hid/KeyboardDevice), because the reports and the descriptors are shared; only the radio differs.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.hidKeyboard().configure(...)` — Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
2. `bluetooth.hidKeyboard().onOutputReport(...)` — Observe a host-to-device HID output report. Check its report ID and length before interpreting fields such as keyboard LED bits.
3. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.

## Verify and adapt

Pair with a compatible host, focus a harmless test window, then press the GPIO 0 button. Key/button presses carry state: release held inputs explicitly and keep report ID, payload length and descriptor consistent. A remembered host may cache the old descriptor after you change the sketch; remove its pairing and reconnect when testing a changed layout.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)

If GPIO 0 shares the board BOOT button, check the board instructions for its use during reset/upload. The example detects a press edge but does not debounce a mechanical contact; add debouncing as appropriate when adapting it.
