# A2dpSinkRaw (Classic)

> 日本語版: [README.ja.md](README.ja.md)

The receiving side of A2DP: a phone or PC streams music to this board. **EspBle
carries already-encoded payloads** — SBC decoding and speaker output belong to
another library, such as PCMFlowBluetooth. **Classic works on the original ESP32
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
