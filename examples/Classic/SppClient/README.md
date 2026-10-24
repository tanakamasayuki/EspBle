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

## Related guides

- [Classic guide §4 SPP](../../../docs/GUIDE_CLASSIC_BASICS.md#4-spp) — RFCOMM channels, several services and the byte stream
- [EspBle in depth](../../../docs/GUIDE_ADVANCED.md) — the 990-byte packet, the eight-write queue and backpressure

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [SppClient.ino](SppClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [SppServer](../SppServer/README.md). For a two-board test, prepare the matching Classic profile on the peer; make the accepting device discoverable when needed and configure the initiating side with its address.

## How this example is structured

The dialling side of SPP. A server waits and is found; a client has to know an address and then find the channel behind it, which is the part examples/Classic/SppServer cannot show.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
2. `bluetooth.spp().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `bluetooth.spp().onConnectionFailed(...)` — Observe failures that happen after a connection request was accepted, including an unavailable peer or a rejected connection.
4. `bluetooth.spp().onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
5. `bluetooth.spp().onData(...)` — Treat SPP input as a byte stream: one callback is not one complete message. Use lengths and your own framing, including for embedded zero bytes.
6. `bluetooth.spp().connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Replace serverAddress; normal connect resolves the SPP channel with SDP, while connectToChannel assumes you already know the channel. Wait for a session event, then frame binary messages over the byte stream.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
