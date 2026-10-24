# A2dpSinkRaw (Classic)

> 日本語版: [README.ja.md](README.ja.md)

The receiving side of A2DP: a phone or PC streams music to this board. **EspBle
carries already-encoded payloads**. For SBC decoding and speaker output, start with
`A2dpSinkM5Speaker` or `A2dpSinkToPcm` in the released
[PCMFlowBluetooth](https://github.com/tanakamasayuki/PCMFlowBluetooth) library.
**Classic works on the original ESP32
only**, and A2DP reaches Classic sources: phones, tablets and PCs. BLE has no
standard audio path in this library, so there is no BLE equivalent.

## Hardware

- 1 × original ESP32 running this sketch
- 1 × A2DP source: a phone, tablet or PC, or a second original ESP32 running
  [A2dpSource](../A2dpSource/)

## What it does

- Accepts a source connection and reports the negotiated media MTU
- Prints the SBC configuration the source chose: sample rate, channels and bitpool
- Counts encoded packets and bytes, printing a line periodically

## Key APIs

- `bluetooth.a2dpSink().begin()` — start the Sink; start
  `bluetooth.avrcp().begin()` first if control is wanted too, as
  [A2dpSinkAvrcp](../A2dpSinkAvrcp/) does
- `onCodecConfigured()` — what the source picked, which a decoder needs
- `onMedia()` — encoded frames; `audio.data` is valid only inside the callback
- `setDelay(tenthsOfMillisecond)` — tell the source how long playback takes here

## Notes

**Copy the payload before returning from `onMedia()`.** The view points at the
backend's buffer, so a decoder running elsewhere needs a bounded queue of copies.
Counting bytes, as this example does, needs no copy.

Only the Sink knows its own latency, so `setDelay()` is a value the application
measures; this library cannot work it out.

## Related guides

- [Classic guide §7 A2DP and AVRCP](../../../docs/GUIDE_CLASSIC_BASICS.md#7-a2dp-and-avrcp) — encoded media, codec configuration and control
- [PCMFlowBluetooth A2DP Sink examples](https://github.com/tanakamasayuki/PCMFlowBluetooth/tree/main/examples) — SBC decoding, PCMFlow and speaker output

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [A2dpSinkRaw.ino](A2dpSinkRaw.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [A2dpSource](../A2dpSource/README.md). For a two-board test, prepare the matching Classic profile on the peer; make the accepting device discoverable when needed and configure the initiating side with its address.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.a2dpSink().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
2. `bluetooth.a2dpSink().onCodecConfigured(...)` — Use the negotiated codec configuration; raw encoded packets are not PCM samples and need a matching external codec pipeline.
3. `bluetooth.a2dpSink().onMedia(...)` — This encoded audio callback runs in the Bluetooth host context. Copy borrowed payload bytes to a bounded queue before returning; decode and play them elsewhere.
4. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.

## Verify and adapt

The received payload is encoded SBC, not PCM, and this sketch only reports it. For audible playback add a decoder, bounded buffering and an audio output pipeline; keep those outside the host callback.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. The stack/host callbacks described above are exceptions: perform only the immediate response or bounded copy they require, rather than assuming they run from update.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
