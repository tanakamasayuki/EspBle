# ProximityServer

> 日本語版: [README.ja.md](README.ja.md)

The Proximity profile **Reporter** role. Hosts two standard services at once: the Link Loss Service (0x1803) with a read/write Alert Level (0x2A06), and the Tx Power Service (0x1804) with a read-only signed-int8 Tx Power Level (0x2A07).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral / Proximity Reporter)
- 1 × central: the [ProximityClient](../ProximityClient/) example, or any Proximity Monitor

## What it does

- Registers both services before `begin()` and advertises 0x1803
- Exposes a fixed Tx Power Level of -8 dBm
- Stores the written Link Loss Alert Level in `onWritten` so it can be read back (a real Reporter would alert on link loss according to this level)

## Key APIs

- Two `addService(...)` calls on one server
- `ble.gattServer().onWritten(...)` — receive Alert Level writes

## Expected Serial output

```
Link Loss Alert Level: 2 (High Alert)
```
