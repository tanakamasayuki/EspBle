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

## Notes

**AVRCP is started before A2DP.** The other order leaves the control channel
unavailable when the source establishes the audio profile.

**A Target may only declare volume-change notifications on this build** — the
bundled Classic host allows nothing else, `supportedNotifications()` reports the
allowed set, and declaring anything else is refused with a message saying so.
Sending metadata or play-status as a Target has no backend API at all. To report
this device's own volume to the source, call
`bluetooth.avrcp().setAbsoluteVolume(value)` — this sketch receives volume
commands but does not send its own reports.

To send keys rather than receive them, this device would be the Controller; see
[AvrcpController](../AvrcpController/).

## Related guides

- [Classic guide §7 A2DP and AVRCP](../../../docs/GUIDE_CLASSIC_BASICS.md#7-a2dp-and-avrcp) — encoded media, codec configuration and control

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [A2dpSinkAvrcp.ino](A2dpSinkAvrcp.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.a2dpSink().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
2. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.

## Verify and adapt

Start AVRCP before the sink so the control channel is ready for the audio peer. AVRCP key and volume events control playback policy; they do not decode or output the incoming A2DP audio.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
