# HidConsumerControl (Classic)

> 日本語版: [README.ja.md](README.ja.md)

Media keys and system requests over Bluetooth Classic (BR/EDR). Car audio units
and older TVs accept Classic HID, so this is the transport for them. Mirrors
[Hid/ConsumerControl](../../Hid/ConsumerControl/) on the BLE side. **Classic
works on the original ESP32 only.**

## Hardware

- 1 × original ESP32 running this sketch
- 1 × Classic HID host that acts on media keys: a car audio unit, a TV, or a PC

## What it does

- Configures Consumer Control and System Control, which are separate profiles
  because a Host treats power and sleep differently from media keys
- Sends volume, play/pause, next track and a sleep request from Serial commands

## Key APIs

- `bluetooth.hidConsumerControl().click(usage)` — press and release; a Host acts
  on the press and needs the release to stop repeating
- `bluetooth.hidSystemControl().click(usage)` — power, sleep and wake requests

## Serial commands

| Key | Effect |
|---|---|
| `+` / `-` | volume up / down |
| `p` | play or pause |
| `n` | next track |
| `s` | sleep request (Generic Desktop usage 0x82) |

## Related guides

- [Classic guide §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.md#6-hid) — the SDP record, the 214-byte budget and what a Host decodes
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — report IDs on Classic, and how to verify a descriptor
- [BLE or Bluetooth Classic](../../../docs/CLASSIC_VS_BLE.md) — how Classic HID differs from BLE HID

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HidConsumerControl.ino](HidConsumerControl.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

Bluetooth Classic (BR/EDR) media keys. Car audio units and older TVs accept Classic HID, so this is the transport for them; the calls are the same ones the BLE example uses (examples/Hid/ConsumerControl).

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.hidConsumerControl().configure(...)` — Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
2. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
3. `bluetooth.hidDevice().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.

## Verify and adapt

Pair with a compatible host, focus a harmless test window, then send the Serial commands shown above. Key/button presses carry state: release held inputs explicitly and keep report ID, payload length and descriptor consistent. A remembered host may cache the old descriptor after you change the sketch; remove its pairing and reconnect when testing a changed layout.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
