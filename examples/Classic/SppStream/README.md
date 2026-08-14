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
