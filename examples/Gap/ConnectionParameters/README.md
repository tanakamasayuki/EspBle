# ConnectionParameters

> 日本語版: [README.ja.md](README.ja.md)

Tunes a connection that is already established.

In BLE you **cannot choose the parameters that decide responsiveness and power draw when connecting**. The connection comes up with values the controller picked, and you request changes afterwards. That asymmetry is the confusing part, so this example prints what was decided at connect time before changing anything.

## The three parameters

| Parameter | Meaning | Unit |
|---|---|---|
| **Connection Interval** | How often the two sides get a chance to talk. Shorter is more responsive and costs more power | 1.25 ms |
| **Peripheral Latency** | How many of those chances the peripheral may skip when it has nothing to send | count |
| **Supervision Timeout** | Silence longer than this counts as a lost link | 10 ms |

The units are the raw spec units: `interval = 24` means 24 × 1.25 = 30 ms.

**The supervision timeout is constrained**: it must exceed `(1 + latency) × maxInterval × 2`. Raising the latency lets the peripheral stay quiet longer, and the timeout must not mistake that for a lost link. A request that violates this is rejected by the peer.

## PHY

The **PHY** is the radio's modulation. Against the default 1M PHY, the **2M PHY** doubles the symbol rate, so the same data takes less airtime. That lowers the energy spent per byte but **shortens the range**.

The PHY cannot be chosen at connect time either, so it is changed the same way.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- A peripheral to connect to — the [Gap/Advertise](../Advertise/) example on a second board, or anything advertising the HID Service (`0x1812`)

## What it does

- Finds and connects to a peer advertising service UUID `0x1812`
- Prints the interval / latency / timeout / PHY **the controller chose** right after connecting
- `f` requests a low-latency profile (interval 15–30 ms, latency 0), `s` a low-power one (interval 400–500 ms, latency 4)
- `p` requests the 2M PHY
- `d` disconnects

## Key APIs

- `ble.updateConnectionParameters(id, minInterval, maxInterval, latency, timeout)` — request a change
- `ble.onConnectionParametersUpdated(callback)` — receive the negotiated result
- `ble.updatePhy(id, txPhyMask, rxPhyMask)` — request a PHY change; masks are `EspBle::Phy1MMask` / `Phy2MMask` / `PhyCodedMask`
- `ble.onPhyUpdated(callback)` — receive the PHY result
- `EspBleConnection` — `connectionInterval` / `peripheralLatency` / `supervisionTimeout` / `txPhy` / `rxPhy`

## Notes

- **The return value only says the request was accepted for sending.** Always read what actually happened from the callback: the peer may answer with different values, or reject the request.
- **Either role may request a change**, but the central's controller has the final say. A peripheral's request only takes effect once the central agrees.
- **The 2M PHY needs support on both sides.** With a peer that lacks it the link stays on 1M: the request still succeeds, and you tell from the unchanged PHY values in the result.
- **Coded PHY (Long Range) depends on the radio.** The ESP32-S3 supports it, but nothing changes if the peer does not.

## Expected Serial output

```
Scanning for a peripheral...
CONNECTED interval=40 (50.00 ms) latency=0 timeout=256 (2560 ms) phy=tx1/rx1
Commands: f fast, s slow, p 2M PHY, d disconnect
REQUEST slow accepted=1
PARAMETERS interval=400 (500.00 ms) latency=4 timeout=600 (6000 ms) phy=tx1/rx1
REQUEST 2M PHY accepted=1
PHY interval=400 (500.00 ms) latency=4 timeout=600 (6000 ms) phy=tx2/rx2
```
