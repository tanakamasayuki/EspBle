# NusClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Central that talks to a Nordic UART Service (NUS) server by composing the generic GATT client API: it subscribes to TX notifications (`6e400003-…`) and sends each Serial line to RX (`6e400002-…`) using Write Without Response, under service `6e400001-…`.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × ESP32-S3 running the [NusServer](../NusServer/) example (or any NUS-compatible peripheral)

## What it does

- Scans for the NUS service UUID and connects
- Subscribes to TX notifications after connecting and prints `NUS ready`
- Reads each non-empty line from Serial and writes it to RX (Write Without Response)
- Prints TX notifications received back from the server as `RX: …`

## Key APIs

- `ble.subscribe(connectionId, serviceUuid, characteristicUuid)` — subscribe to TX notifications
- `ble.onSubscribed(callback)` — subscription completion (`result.success`)
- `ble.writeCharacteristic(connectionId, serviceUuid, characteristicUuid, value, false)` — Write Without Response to RX
- `ble.onCharacteristicWritten(callback)` — write acceptance result
- `ble.onNotification(callback)` — TX payload, filtered by `characteristicUuid.equalsIgnoreCase(...)`

## Notes

- This shows NUS built purely from the generic GATT client API; no stream semantics or automatic packet framing are implied. Type a line in the Serial monitor to send it.

## Expected Serial output

```
NUS ready: 1
TX accepted: 1
RX: hello
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms
