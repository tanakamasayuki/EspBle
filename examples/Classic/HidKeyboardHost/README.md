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

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HidKeyboardHost.ino](HidKeyboardHost.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [HidKeyboard](../HidKeyboard/README.md). For a two-board test, prepare the matching Classic profile on the peer; make the accepting device discoverable when needed and configure the initiating side with its address.

## How this example is structured

A Classic HID host that receives decoded key and mouse events. The callbacks and event types are the ones the BLE host uses (examples/Hid/KeyboardHost); only the radio and the addressing differ.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `keyboard.onInputReport(...)` — Inspect an incoming HID report using its descriptor, report ID and length; vendor data does not automatically become a keyboard event.
2. `keyboard.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `keyboard.onConnectionFailed(...)` — Observe failures that happen after a connection request was accepted, including an unavailable peer or a rejected connection.
4. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
5. `bluetooth.hidHost().connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Use a matching HID device and wait for profile discovery/readiness, not just the radio connection. Route each report according to its ID and descriptor; sending keyboard LED/output reports is the host-to-device direction, independent of receiving key input.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
