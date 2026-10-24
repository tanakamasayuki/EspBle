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

## Related guides

- [Classic guide §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.md#6-hid) — the SDP record, the 214-byte budget and what a Host decodes
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — report IDs on Classic, and how to verify a descriptor
- [BLE or Bluetooth Classic](../../../docs/CLASSIC_VS_BLE.md) — how Classic HID differs from BLE HID

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HidGamepad.ino](HidGamepad.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

A Bluetooth Classic (BR/EDR) HID gamepad. Older game consoles and PCs only accept Classic HID, so BLE is not an alternative for them; the calls are the same ones the BLE example uses (examples/Hid/Gamepad).

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.hidGamepad().configure(...)` — Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
2. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
3. `bluetooth.hidDevice().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
4. `bluetooth.hidDevice().onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.

## Verify and adapt

Pair with a compatible host, focus a harmless test window, then send the Serial commands shown above. Key/button presses carry state: release held inputs explicitly and keep report ID, payload length and descriptor consistent. A remembered host may cache the old descriptor after you change the sketch; remove its pairing and reconnect when testing a changed layout.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
