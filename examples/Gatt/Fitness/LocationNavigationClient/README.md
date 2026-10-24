# LocationNavigationClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Central / GATT client for the Location and Navigation Service (0x1819). It reads LN Feature (0x2A6A), subscribes to Location and Speed (0x2A67) notifications, and decodes Instantaneous Speed and the Location latitude/longitude.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × peripheral: the [LocationNavigationServer](../LocationNavigationServer/) example

## What it does

- Scans for a connectable peer advertising 0x1819 and connects
- On connect, reads the 4-byte LN Feature, then subscribes to Location and Speed
- Decodes only measurements carrying both Instantaneous Speed (bit 0) and Location (bit 2): speed (1/100 m/s) and latitude/longitude (1e-7 degrees)

## Key APIs

- `ble.readCharacteristic(...)` — read LN Feature
- `ble.subscribe(...)` — enable Location and Speed notifications
- `ble.onNotification(callback)` — decode speed and location

## Expected Serial output

```
Speed: 5.10 m/s, location: 35.681200, 139.767100
Speed: 5.20 m/s, location: 35.681200, 139.767100
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [LocationNavigationClient.ino](LocationNavigationClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [LocationNavigationServer](../LocationNavigationServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

LocationNavigationClient - connect to a Location and Navigation Service (0x1819), read LN Feature, subscribe to Location and Speed notifications, and decode Instantaneous Speed and the Location latitude/longitude.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.
4. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
5. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
6. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
7. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
8. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Latitude/longitude are signed scaled integers. Preserve sign and scale and inspect flags before reading position fields; the demonstration coordinates do not come from a GNSS receiver.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
