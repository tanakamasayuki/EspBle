# A2dpSinkAvrcp (Classic)

> 日本語版: [README.ja.md](README.ja.md)

A2DP Sink plus AVRCP, which is the pair a speaker or headset needs: A2DP carries
the music and AVRCP carries the buttons and the volume. **Classic works on the
original ESP32 only**, and both profiles reach Classic peers — phones, tablets and
PCs. This side is the Target: the phone presses play, this board hears about it.

## Hardware

- 1 × original ESP32 running this sketch
- 1 × A2DP source with AVRCP: a phone, tablet or PC

## What it does

- Starts AVRCP before A2DP, which the backend requires
- Prints the passthrough keys the source sends, press and release separately
- Prints volume changes, distinguishing one the remote commanded from a local
  change
- Reports the A2DP connection

## Key APIs

- `bluetooth.avrcp().begin()` — **before** `a2dpSink().begin()`
- `onPassthrough()` — play, pause, next and the rest, as key press and release
- `onVolumeChanged()` — `remoteCommand` tells a commanded change from a reported
  one
- `bluetooth.avrcp().setAbsoluteVolume(value)` — report this device's volume

## Notes

**AVRCP is started before A2DP.** The other order leaves the control channel
unavailable when the source establishes the audio profile.

**A Target may only declare volume-change notifications on this build** — the
bundled Classic host allows nothing else, `supportedNotifications()` reports the
allowed set, and declaring anything else is refused with a message saying so.
Sending metadata or play-status as a Target has no backend API at all.

To send keys rather than receive them, this device would be the Controller; see
[AvrcpController](../AvrcpController/).

## Related guides

- [Classic guide §7 A2DP and AVRCP](../../../docs/GUIDE_CLASSIC_BASICS.md#7-a2dp-and-avrcp) — encoded media, codec configuration and control
