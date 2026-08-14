# MultiConnection

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md)

One Central holding several Peripheral connections at once. Each connection has
its own ID, and every operation names the connection it applies to — nothing in
this library is implicitly "the current connection".

## Hardware

- 1 × ESP32-S3 running this sketch (Central)
- 2–3 × peripherals advertising the Battery Service `0x180f`, for example boards
  running [Gatt/Device/BatteryServer](../../Gatt/Device/BatteryServer/)

## What it does

- Resumes scanning after each connection instead of stopping at the first peer,
  and stops collecting once it is full — a full Central would otherwise keep
  trying connections the host refuses
- Reads the battery level from every peer in turn; the shared result callback
  reports which connection each answer came from
- Removes the right entry on disconnect by matching the ID, which belongs to one
  connection for its whole life

## Key APIs

- `ble.connectionCount()` — how many are held right now
- `ble.readCharacteristic(connectionId, service, characteristic)` — the
  connection is part of the call
- `ble.onCharacteristicRead()` — one callback for every connection; the result
  carries the connection ID

## Limits

Three simultaneous connections on the original ESP32's bundled host. Other
targets allow more.

## Serial commands

| Key | Effect |
|---|---|
| `r` | read the battery level from every peer |
| `d` | disconnect the first peer |

## Related guides

- [BLE guide §2 GAP](../../../docs/GUIDE_BLE_BASICS.md#2-gap--finding-and-connecting) — advertising, scanning and connecting
