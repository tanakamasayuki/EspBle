# ProximityClient

> 日本語版: [README.ja.md](README.ja.md)

The Proximity profile **Monitor** role. Connects to a Proximity Reporter, reads its Tx Power Level (0x2A07, signed int8), reads the Link Loss Alert Level (0x2A06), and writes a Link Loss Alert Level so the Reporter alerts if the link is later lost.

## Hardware

- 1 × ESP32-S3 running this sketch (central / Proximity Monitor)
- 1 × Proximity Reporter: the [ProximityServer](../ProximityServer/) example

## What it does

- Scans for and connects to a device advertising 0x1803 (Link Loss)
- Reads Tx Power Level (0x2A07) as a signed int8 (dBm)
- Reads the current Link Loss Alert Level (0x2A06)
- Writes Link Loss Alert Level = 2 (High Alert) **with response** to arm link-loss alerting

## Key APIs

- `ble.readCharacteristic(...)` across two different services
- `ble.writeCharacteristic(..., true)` — write with response

## Expected Serial output

```
Tx Power Level: -8 dBm
Link Loss Alert Level: 0
Armed Link Loss High Alert
```
