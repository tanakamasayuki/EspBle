# AvrcpController (Classic)

> 日本語版: [README.ja.md](README.ja.md)

AVRCP Controller: the side that presses play and pause on the other device.
[A2dpSinkAvrcp](../A2dpSinkAvrcp/) shows the Target side, which receives those
presses. AVRCP carries control only — the audio travels over A2DP, and the AVRCP
connection follows the A2DP one. **Classic works on the original ESP32 only.**

## Hardware

- 1 × original ESP32 running this sketch
- 1 × player: a phone, or a second original ESP32 running
  [A2dpSinkAvrcp](../A2dpSinkAvrcp/)

Put the player's address in `playerAddress`; [Inquiry](../Inquiry/) finds one.

## What it does

- Starts AVRCP before A2DP, which the backend requires
- Runs the Controller role only; a device can run both, as A2dpSinkAvrcp does
- Sends playback keys and reports whether the other device accepted them —
  `accepted=0` means it understood the command and refused, which is not the same
  as the command never arriving
- Asks for play status and metadata, and sets absolute volume and a player
  setting

## Key APIs

- `bluetooth.avrcp().sendKey(command)` / `onPassthroughResponse()`
- `requestPlayStatus()` / `onPlayStatus()`
- `requestMetadata(mask)` / `onMetadata()`
- `setAbsoluteVolume(0..127)` — not a percentage
- `setPlayerSetting(attribute, value)` — repeat, shuffle and similar

## Serial commands

| Key | Effect |
|---|---|
| `c` | connect media, which brings AVRCP with it |
| `p` / `x` | play / pause |
| `n` / `b` | next / previous track |
| `i` | ask for play status |
| `m` | ask for title, artist and album |
| `v` | set absolute volume to 64 |
| `r` | repeat one track |

## Related guides

- [Classic guide §7 A2DP and AVRCP](../../../docs/GUIDE_CLASSIC_BASICS.md#7-a2dp-and-avrcp) — encoded media, codec configuration and control

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [AvrcpController.ino](AvrcpController.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

AVRCP Controller: the side that presses play and pause on the other device. examples/Classic/A2dpSinkAvrcp shows the Target side, which receives those presses. AVRCP carries control only — the audio travels over A2DP, and the AVRCP connection follows the A2DP one.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
2. `bluetooth.a2dpSource().connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Replace the speaker address and establish the required profile connection before sending controls. Metadata/control responses are independent of the audio stream; successful command submission is not evidence of audible playback.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
