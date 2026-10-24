# ProximityClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

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

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [ProximityClient.ino](ProximityClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [ProximityServer](../ProximityServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

ProximityClient - the Proximity profile Monitor role. Connect to a Proximity Reporter, read its Tx Power Level (0x2A07, signed int8), read the Link Loss Alert Level (0x2A06), and write a Link Loss Alert Level so the Reporter alerts if the link is later lost.

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

Tx Power is a declared radio level and Link Loss Alert Level is configuration. They do not provide a calibrated distance measurement; implement the physical link-loss action separately if needed.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
