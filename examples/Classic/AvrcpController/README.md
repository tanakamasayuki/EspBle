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
