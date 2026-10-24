# A2dpSource (Classic)

> 日本語版: [README.ja.md](README.ja.md)

A2DP Source: this device sends audio to a speaker or headset. EspBle carries
already-encoded SBC frames — it does not encode — so the frames here come from a
fixed table. A real sketch uses `SbcEncoder` from the released
[PCMFlowBluetooth](https://github.com/tanakamasayuki/PCMFlowBluetooth) library;
its `A2dpSourceM5Microphone` example is a complete M5Stack Core2 microphone
transmitter. The receiving side is [A2dpSinkRaw](../A2dpSinkRaw/).
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

## Related guides

- [Classic guide §7 A2DP and AVRCP](../../../docs/GUIDE_CLASSIC_BASICS.md#7-a2dp-and-avrcp) — encoded media, codec configuration and control
- [PCMFlowBluetooth A2DP Source example](https://github.com/tanakamasayuki/PCMFlowBluetooth/tree/main/examples/A2dpSourceM5Microphone) — microphone capture, SBC encoding and backpressure-safe transmission

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [A2dpSource.ino](A2dpSource.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [A2dpSinkRaw](../A2dpSinkRaw/README.md). For a two-board test, prepare the matching Classic profile on the peer; make the accepting device discoverable when needed and configure the initiating side with its address.

## How this example is structured

A2DP Source: this device sends audio to a speaker or headset. EspBle carries already-encoded SBC frames — it does not encode, so the frames here come from a table. A real sketch takes them from an encoder such as PCMFlowBluetooth. The receiving side is examples/Classic/A2dpSinkRaw.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
2. `bluetooth.a2dpSource().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `bluetooth.a2dpSource().onCodecConfigured(...)` — Use the negotiated codec configuration; raw encoded packets are not PCM samples and need a matching external codec pipeline.
4. `bluetooth.a2dpSource().onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
5. `bluetooth.a2dpSource().connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

The sample sends already encoded frames from a table. A real encoder must follow the negotiated codec configuration. Keep a pending frame when send returns WouldBlock and retry it; silently dropping it creates a gap.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
