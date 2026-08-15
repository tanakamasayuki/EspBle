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
