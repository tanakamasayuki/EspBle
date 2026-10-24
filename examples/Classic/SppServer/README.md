# SppServer (Classic)

> 日本語版: [README.ja.md](README.ja.md)

The waiting side of SPP: a binary-safe byte stream over Classic that a PC or
Android phone sees as a serial port. **Classic works on the original ESP32 only**,
and SPP reaches Classic peers — PCs, Android devices, and other ESP32s. **iOS
apps cannot use SPP** (MFi accessories aside), so for an iPhone use BLE with a
GATT service of your own. For code written against `Serial`, see
[SppStream](../SppStream/).

## Hardware

- 1 × original ESP32 running this sketch
- 1 × SPP client: a PC or Android device with a serial terminal, or a second
  original ESP32 running [SppClient](../SppClient/)

## What it does

- Publishes one SPP service and waits
- Echoes whatever arrives, byte for byte
- Tracks the open session so a sketch knows where to write

## Key APIs

- `bluetooth.spp().startServer()` — publish a service; call it again to publish
  up to four, each on its own RFCOMM channel
- `onConnected()` / `onDisconnected()` — the session id, which every read and
  write names
- `onData()` — received bytes as they arrive; `available()` / `read()` is the
  same buffer seen as a stream
- `write(sessionId, value)` — queued, not sent: delivery is reported at
  `onWriteCompleted()`

## Notes

SPP is binary-safe: `0x00` in the middle of a payload is data, not a terminator.

Pairing is not required for SPP by itself, but a peer may insist. To control it
from the sketch, see [SppPairing](../SppPairing/).

## Related guides

- [Classic guide §4 SPP](../../../docs/GUIDE_CLASSIC_BASICS.md#4-spp) — RFCOMM channels, several services and the byte stream
- [EspBle in depth](../../../docs/GUIDE_ADVANCED.md) — the 990-byte packet, the eight-write queue and backpressure

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [SppServer.ino](SppServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [SppClient](../SppClient/README.md). For a two-board test, prepare the matching Classic profile on the peer; make the accepting device discoverable when needed and configure the initiating side with its address.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
2. `bluetooth.spp().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `bluetooth.spp().onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
4. `bluetooth.spp().onData(...)` — Treat SPP input as a byte stream: one callback is not one complete message. Use lengths and your own framing, including for embedded zero bytes.

## Verify and adapt

Start a named SPP server and connect from a Classic SPP client. Received chunks may split or combine application messages; keep a bounded parser when extending the echo behavior.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
