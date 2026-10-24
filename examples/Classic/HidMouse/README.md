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

## Related guides

- [Classic guide §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.md#6-hid) — the SDP record, the 214-byte budget and what a Host decodes
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — report IDs on Classic, and how to verify a descriptor
- [BLE or Bluetooth Classic](../../../docs/CLASSIC_VS_BLE.md) — how Classic HID differs from BLE HID

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HidMouse.ino](HidMouse.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

A Bluetooth Classic (BR/EDR) HID mouse. The calls are the same ones the BLE example uses (examples/Hid/Mouse); only the radio differs.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.hidMouse().configure(...)` — Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
2. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
3. `bluetooth.hidDevice().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.

## Verify and adapt

Pair with a compatible host, focus a harmless test window, then send the Serial commands shown above. Key/button presses carry state: release held inputs explicitly and keep report ID, payload length and descriptor consistent. A remembered host may cache the old descriptor after you change the sketch; remove its pairing and reconnect when testing a changed layout.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
