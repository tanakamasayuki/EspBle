# ImmediateAlertClient

> 日本語版: [README.ja.md](README.ja.md)

The Find Me profile **locator** role. Connects to an Immediate Alert Service (0x1802) and writes Alert Level (0x2A06) with **Write Without Response** to make the target alert.

## Hardware

- 1 × ESP32-S3 running this sketch (central / Find Me locator)
- 1 × Immediate Alert peripheral: the [ImmediateAlertServer](../ImmediateAlertServer/) example

## What it does

- Scans for and connects to a device advertising 0x1802
- On connect, writes Alert Level = 2 (High Alert) with Write Without Response
- Five seconds later, writes Alert Level = 0 (No Alert)

## Key APIs

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, false)` — Write Without Response (last argument false)

## Expected Serial output

```
High Alert sent
No Alert sent
```
