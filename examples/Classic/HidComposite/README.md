# HidComposite (Classic)

> 日本語版: [README.ja.md](README.ja.md)

One Bluetooth Classic (BR/EDR) HID device that is a keyboard, a mouse and media
keys at once. Classic registers a single device record, so all of it goes into
one composed Report Descriptor and each profile keeps its own report ID.

**How many profiles fit is limited here in a way it is not on BLE.** The Report
Descriptor and the three profile strings share one SDP record and may total 214
bytes; these three profiles come to 144 descriptor bytes and the strings to 57,
so 201 fits. Adding the gamepad makes the descriptor 212 and nothing registers. `begin()` refuses such a combination rather than starting a device no
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

## Related guides

- [Classic guide §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.md#6-hid) — the SDP record, the 214-byte budget and what a Host decodes
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — report IDs on Classic, and how to verify a descriptor
- [BLE or Bluetooth Classic](../../../docs/CLASSIC_VS_BLE.md) — how Classic HID differs from BLE HID

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HidComposite.ino](HidComposite.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

One Bluetooth Classic (BR/EDR) HID device that is a keyboard, a mouse and media keys at once. Classic registers a single device record, so all of it goes into one composed Report Descriptor and each profile keeps its own report ID. Mirrors examples/Hid/CompositeKeyboardMouse on the BLE side.  How many profiles fit is limited here in a way it is not on BLE: the Report Descriptor and the three profile strings share one SDP record and may total 214 bytes. These three come to 144 descriptor bytes and the strings below to 57, so 201 fits. Adding the gamepad makes the descriptor 212 and nothing registers — see examples/Classic/HidGamepad for that one on its own.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.hidKeyboard().configure(...)` — Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
2. `bluetooth.hidKeyboard().onOutputReport(...)` — Observe a host-to-device HID output report. Check its report ID and length before interpreting fields such as keyboard LED bits.
3. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
4. `bluetooth.hidDevice().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.

## Verify and adapt

Pair with a compatible host, focus a harmless test window, then send the Serial commands shown above. Key/button presses carry state: release held inputs explicitly and keep report ID, payload length and descriptor consistent. A remembered host may cache the old descriptor after you change the sketch; remove its pairing and reconnect when testing a changed layout.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
