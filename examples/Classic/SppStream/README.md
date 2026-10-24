# SppStream (Classic)

> 日本語版: [README.ja.md](README.ja.md)

SPP through an Arduino `Stream`, for code written against `Serial`. The session is
still opened and closed with the SPP API — the Stream only borrows it — so
`print()`, `readStringUntil()` and `parseInt()` work over Bluetooth without
giving up the session events. **Classic works on the original ESP32 only.**

## Hardware

- 1 × original ESP32 running this sketch
- 1 × SPP client: a PC or Android device with a serial terminal, or a second
  original ESP32 running [SppClient](../SppClient/)

## What it does

- Attaches the Stream when a session opens and detaches it when the session
  closes, because a Stream has no connection of its own
- Reads a line at a time with `readStringUntil()` and echoes it back
- `flush()` waits for the queued bytes to reach the peer, bounded by the write
  timeout

## Key APIs

- `EspBleClassicSppStream stream;` — an unattached Stream
- `stream.attach(bluetooth.spp(), session.id)` / `stream.detach()`
- `stream.setTimeout(ms)` — the read side, as on `Serial`
- `stream.setWriteTimeout(ms)` — how long a write waits for queue room; 0 never
  waits
- `stream.connected()` / `stream.session()` — whether the borrowed session is
  still open, and which one it is

## Notes

Two things differ from `Serial` and are worth knowing:

- **A write becomes one SPP packet.** `println(line)` is one packet;
  `write(byte)` in a loop is one packet per byte. Write lines, not characters
- **The outgoing queue is finite.** A write with no room waits up to the write
  timeout and then reports how much it took, so check the return value in a
  sketch that sends faster than the link drains

`available()` and `read()` read the same session buffer the SPP API exposes, so
mixing `stream.read()` with `spp().read(sessionId)` is allowed — they are two
views of one buffer, not two buffers.

## Related guides

- [Classic guide §4 SPP](../../../docs/GUIDE_CLASSIC_BASICS.md#4-spp) — RFCOMM channels, several services and the byte stream
- [EspBle in depth](../../../docs/GUIDE_ADVANCED.md) — the 990-byte packet, the eight-write queue and backpressure

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [SppStream.ino](SppStream.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

SPP through an Arduino Stream, for code written against Serial. The session is still opened and closed with the SPP API; the Stream only borrows it, so print(), readStringUntil() and parseInt() work over Bluetooth without giving up the session events.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
2. `bluetooth.spp().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `bluetooth.spp().onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.

## Verify and adapt

Attach the Stream wrapper to the live session and detach on disconnect. Delimited reads and flush/write timeouts can block loop; keep update running and define a maximum line length when adapting it.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
