# BondManagementClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to a Bond Management Service (0x181E), reads the Bond Management Feature bit field, and writes the Bond Management Control Point op code "Delete bond of requesting device (LE)" (0x03) with response.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Bond Management peripheral: the [BondManagementServer](../BondManagementServer/) example

## What it does

- Scans for and connects to a device advertising 0x181E
- Reads the Bond Management Feature (0x2AA5) and prints the supported-operations bit field
- Writes the Bond Management Control Point (0x2AA4) op code 0x03 (Delete bond of requesting device, LE) **with response**

## Key APIs

- `ble.writeCharacteristic(connectionId, service, characteristic, data, length, true)` — write with response

## Expected Serial output

```
Bond Management Feature: 0x000011
Delete-bond op code sent
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [BondManagementClient.ino](BondManagementClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [BondManagementServer](../BondManagementServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

BondManagementClient - connect to a Bond Management Service (0x181E), read the Bond Management Feature bit field, and write the Bond Management Control Point op code "Delete bond of requesting device (LE)" (0x03) with response.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.
4. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
5. `ble.writeCharacteristic(...)` — Queue a remote write and inspect its completion event. Write Without Response provides no ATT response proving the peer application acted on the data.
6. `ble.onCharacteristicWritten(...)` — Check result.success for the write outcome at the selected write mode; use a separate application response if you need confirmation of a command action.
7. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
8. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

The Control Point changes stored bonds rather than merely changing a displayed setting. Inspect the supported Feature bits and the exact opcode implemented in the sketch; do not treat the example as authorization policy for an exposed management service.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
