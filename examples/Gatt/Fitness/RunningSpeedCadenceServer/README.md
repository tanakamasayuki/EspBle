# RunningSpeedCadenceServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Running Speed and Cadence Service (0x1814) peripheral. RSC Measurement (0x2A53) is notified with instantaneous speed and cadence plus optional stride length and total distance; RSC Feature (0x2A54) and Sensor Location (0x2A5D) are readable.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [RunningSpeedCadenceClient](../RunningSpeedCadenceClient/) example, or any RSC collector

## What it does

- Registers the RSC service and advertises 0x1814
- Publishes RSC Feature = Stride + Distance (0x0003) and Sensor Location = In Shoe (2) as readable values
- Every second, notifies a 10-byte RSC Measurement (flags 0x03) at 3.0 m/s, cadence 180, stride 1.25 m, and a growing total distance (+3.0 m)

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .notifiable = true })` — RSC Measurement
- `ble.gattServer().setValue(...)` — seed the readable Feature / Sensor Location
- `ble.gattServer().notify(...)` — push each measurement to subscribers

## Notes

- Speed is in 1/256 m/s units, stride length in 1/100 m, and total distance in 1/10 m. Flags bit 2 (walking/running) is left 0, so the reading is reported as walking.

## Expected Serial output

```
The server is silent; observe values on the client.
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms
