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

## Related guides

- [Classic guide §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.md#6-hid) — the SDP record, the 214-byte budget and what a Host decodes
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — report IDs on Classic, and how to verify a descriptor
- [BLE or Bluetooth Classic](../../../docs/CLASSIC_VS_BLE.md) — how Classic HID differs from BLE HID

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HidKeyboardNkro.ino](HidKeyboardNkro.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

A Bluetooth Classic (BR/EDR) keyboard with N-key rollover: every key is a bit, so there is no six-key limit. The calls are the same ones the BLE example uses (examples/Hid/KeyboardNkro).

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.hidKeyboard().configure(...)` — Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
2. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
3. `bluetooth.hidKeyboard().onOutputReport(...)` — Observe a host-to-device HID output report. Check its report ID and length before interpreting fields such as keyboard LED bits.

## Verify and adapt

Pair with a compatible host, focus a harmless test window, then send the Serial commands shown above. Key/button presses carry state: release held inputs explicitly and keep report ID, payload length and descriptor consistent. A remembered host may cache the old descriptor after you change the sketch; remove its pairing and reconnect when testing a changed layout.

Enable NKRO before configuring the keyboard and use the NKRO whole-state report when sending more than six keys. For BLE, verify the negotiated MTU is large enough for that report before testing the chord.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
