# DirectedAdvertise

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 2, "GAP"

A peripheral-side example that **advertises to exactly one peer**.

Where ordinary advertising broadcasts "anyone may connect", **directed advertising** names the target address in the PDU, so **only that peer may connect**. Its main use is reconnecting quickly to a bonded peer.

Its defining property is that it **cannot carry a payload at all**. By specification, a directed advertising PDU carries only two addresses: the sender's and the target's. No name, no service UUID. The peer therefore does not scan for this device — it **connects by address**.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- A central that connects — the [Gap/Connect](../Connect/) example on a second board, or a phone app

Replace `TARGET_CENTRAL` in the sketch with **the address of the central to advertise to**, and `TARGET_TYPE` with its address type. That board can report both with `ble.localAddress()` / `ble.localAddressType()`.

## What it does

- Starts **undirected**, so the central can find this device once and learn its address
- Sending `d` switches to **directed** advertising aimed at `TARGET_CENTRAL`. No payload is transmitted
- The central connects **by address** (`ble.connect(address, addressType)`) rather than from a scan result
- Sending `u` returns to undirected advertising. The payload was kept while directed, just not transmitted

## Key APIs

- `ble.advertising().setDirectedTarget(address, addressType, highDuty)` — set the target
- `ble.advertising().clearDirectedTarget()` — return to normal advertising
- `ble.localAddress()` / `ble.localAddressType()` — how each side tells the other what to target

## Notes

- **No payload is sent.** Not the name, not service UUIDs, not manufacturer data. This is the BLE specification, not a library limitation.
- **If the peer uses an RPA (Resolvable Private Address), give its identity address.** Resolution goes through the bond, so the peer **must be bonded first** (see [Gap/PrivateAddress](../PrivateAddress/) and [Security/JustWorksServer](../../Security/JustWorksServer/)).
- **High Duty Cycle (third argument `true`) stops by itself after 1.28 s.** It advertises every 3.75 ms, which reconnects to a known peer as fast as possible, but it cannot run for long. The default `false` follows `setInterval()` and advertises until `stop()`.
- **Advertising stops once a connection is established.** Call `start()` again from `onDisconnected` to keep going.
- If you only want to restrict who may connect and do not need the fast reconnection, ordinary advertising plus [Gap/AcceptList](../AcceptList/) is easier to work with — the peer can still find this device by scanning.

## Expected Serial output

```
Advertising as d0:cf:13:58:fd:94. Send 'd' to direct it at aa:bb:cc:dd:ee:ff.
Directed at aa:bb:cc:dd:ee:ff. No payload is sent.
Connected id=1 from aa:bb:cc:dd:ee:ff
Disconnected id=1
Undirected: anyone may connect.
```

## Related guides

- [BLE guide §2 GAP](../../../docs/GUIDE_BLE_BASICS.md#2-gap--finding-and-connecting) — advertising, scanning and connecting
