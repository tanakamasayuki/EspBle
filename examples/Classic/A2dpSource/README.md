# A2dpSource (Classic)

> 日本語版: [README.ja.md](README.ja.md)

A2DP Source: this device sends audio to a speaker or headset. EspBle carries
already-encoded SBC frames — it does not encode — so the frames here come from a
fixed table and a real sketch takes them from an encoder such as
PCMFlowBluetooth. The receiving side is [A2dpSinkRaw](../A2dpSinkRaw/).
**Classic works on the original ESP32 only.**

## Hardware

- 1 × original ESP32 running this sketch
- 1 × A2DP Sink: a Bluetooth speaker, or a second original ESP32 running
  [A2dpSinkRaw](../A2dpSinkRaw/)

Put the speaker's address in `speakerAddress`; [Inquiry](../Inquiry/) finds one.

## What it does

- Reports the negotiated codec, which the Sink chooses from what this Source
  offered — the encoder has to follow that, not the other way round
- Sends one frame per loop while streaming
- Treats `WouldBlock` as backpressure rather than an error: the frame has to be
  kept and retried, or the stream develops gaps
- Prints the delay the Sink reports for itself, which a Source showing video uses
  to hold the picture back by the same amount

## Key APIs

- `bluetooth.a2dpSource().begin()` / `connect(address)` / `start()` / `suspend()`
- `send(packet)` — returns `Accepted`, `WouldBlock`, or a failure
- `onSinkDelay()` — the Sink's own playback latency, in tenths of a millisecond

## Serial commands

| Key | Effect |
|---|---|
| `c` | connect to the speaker |
| `s` | start streaming |
| `p` | suspend |
| `d` | disconnect |
