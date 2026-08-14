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
