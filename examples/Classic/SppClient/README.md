# SppClient (Classic)

> 日本語版: [README.ja.md](README.ja.md)

The dialling side of SPP. A server waits and is found; a client has to know an
address and then find the RFCOMM channel behind it — the part
[SppServer](../SppServer/) cannot show. **Classic works on the original ESP32
only.**

## Hardware

- 1 × original ESP32 running this sketch (client)
- 1 × SPP server: a second original ESP32 running [SppServer](../SppServer/), or
  a PC or Android device offering a serial service

Put the server's address in `serverAddress`; [Inquiry](../Inquiry/) finds one.

## What it does

- `connect(address)` asks the peer over SDP which channel its SPP service is on
- `connectToChannel(address, channel)` skips discovery for a channel the caller
  already knows, which is what a peer publishing several services needs
- Reports connection failures separately from connection success, because
  `connect()` returning true only means the attempt started
- Sends bytes including `0x00`, which SPP carries like any other value

## Key APIs

- `bluetooth.spp().connect(address)` — resolve the channel and connect
- `bluetooth.spp().connectToChannel(address, channel)` — connect to a named
  channel
- `onConnectionFailed()` — the attempt failed after being accepted
- `write(sessionId, data, length)` / `onData()` — a byte stream, not messages

## Serial commands

| Key | Effect |
|---|---|
| `c` | connect, resolving the channel over SDP |
| `k` | connect to channel 1 directly |
| `w` | send four bytes including a zero |
| `d` | disconnect |
