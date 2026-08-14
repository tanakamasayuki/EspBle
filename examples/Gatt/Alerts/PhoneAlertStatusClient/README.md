# PhoneAlertStatusClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to a Phone Alert Status Service (0x180E), reads Alert Status, subscribes to Ringer Setting, and drives the Ringer Control Point (**Write Without Response**) to set Silent Mode then cancel it, printing the notified Ringer Setting each time.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Phone Alert Status peripheral: the [PhoneAlertStatusServer](../PhoneAlertStatusServer/) example

## What it does

- Scans for and connects to a device advertising 0x180E
- Reads Alert Status (0x2A3F) and prints the bitmask
- Subscribes to Ringer Setting (0x2A41) **notifications**
- Every 5 seconds, toggles the Ringer Control Point (0x2A40) between Set Silent Mode (1) and Cancel Silent Mode (3), and prints each notified Ringer Setting

## Key APIs

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, false)` — Write Without Response

## Expected Serial output

```
Alert Status: 0x01
Ringer Setting: Silent
Ringer Setting: Normal
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms
