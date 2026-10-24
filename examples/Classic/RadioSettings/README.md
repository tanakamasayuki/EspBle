# RadioSettings (Classic)

> 日本語版: [README.ja.md](README.ja.md)

The three radio and link settings a Classic sketch can choose: transmit power,
page timeout and the minimum encryption key size. None of them change what a
profile does, so this sketch shows them through a connection attempt — a shorter
page timeout is how long `connect()` takes to give up on a peer that is not
there. **Classic works on the original ESP32 only.**

## Hardware

- 1 × original ESP32 running this sketch

No peer is needed: `absentAddress` is an address nothing answers on, which is
what makes the page timeout visible. Put a real address there to watch a
connection succeed instead.

## What it does

- Sets the BR/EDR transmit power as a range, because power control picks a level
  per packet from within it. The single-value form pins both ends
- Shortens the page timeout to 1000 ms before connecting, so an attempt at an
  absent peer ends in about a second instead of the default 5120 ms
- Refuses encryption keys shorter than 16 bytes
- Prints what the radio applied, which is the value rounded to a supported level

## Key APIs

- `bluetooth.setTxPower(minimum, maximum)` / `setTxPower(dBm)` — -12..+9 dBm in
  3 dB steps; separate from `EspBle::setTxPower()`, which sets the LE power
- `bluetooth.txPower(minimum, maximum)` — the range the radio applied
- `bluetooth.setPageTimeout(milliseconds)` — 14..40959 ms, default 5120, applied
  from the next page onwards
- `bluetooth.pageTimeout()` — the confirmed value, or 0 until the backend
  confirms one
- `bluetooth.setMinimumEncryptionKeySize(bytes)` — 7..16, for links established
  afterwards

## Serial commands

| Key | Effect |
|---|---|
| `p` | print the transmit power range and the page timeout |
| `c` | try to connect to `absentAddress` and time how long the failure takes |

## Notes

`setPageTimeout()` returning true means the request was accepted. The backend
confirms it on its own task, so `pageTimeout()` reads as 0 until the
confirmation arrives — including the read the library issues at startup to learn
the controller's default.

A shorter page timeout gives up on a peer that was merely slow to answer, and a
lower transmit power shortens the range. Both are trades, not improvements.

## Related guides

- [Classic guide §2 Starting up and being visible](../../../docs/GUIDE_CLASSIC_BASICS.md#2-starting-up-and-being-visible) — transmit power, page timeout and discoverability

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [RadioSettings.ino](RadioSettings.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

The three radio and link settings a Classic sketch can choose: transmit power, page timeout and the minimum encryption key size. None of them change what a profile does, so they are easiest to see through a connection attempt: a shorter page timeout is how long connect() takes to give up on a peer that is not there.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
2. `bluetooth.spp().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `bluetooth.spp().onConnectionFailed(...)` — Observe failures that happen after a connection request was accepted, including an unavailable peer or a rejected connection.
4. `bluetooth.spp().connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Change one radio/security parameter at a time and compare the reported result. A stronger transmit setting or longer page timeout cannot add a missing peer service, and encryption-key policy can reject otherwise discoverable peers.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
