# Connect

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 2, "GAP"

Scans for a peripheral advertising a specific service UUID and connects to it as a central. Demonstrates the asynchronous connection model: `connect()` only accepts the request, and completion (or failure) arrives later as an event delivered from `ble.update()`.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × BLE peripheral advertising the target service UUID — e.g. a second board running the [Gatt/Basics/Server](../../Gatt/Basics/Server/) example (change `TARGET_SERVICE_UUID` to match)

## What it does

- Starts an active scan and looks for `TARGET_SERVICE_UUID` in each result
- Stops the scan and requests a connection to the first match
- Prints connect, disconnect, and connection-failure events with the library connection ID
- Retries on the next scan result after a disconnect or failure

Edit `TARGET_SERVICE_UUID` at the top of the sketch to the UUID your peripheral advertises.

## Key APIs

- `scanResult.advertisesService(uuid)` — match either the 16-bit form (`"1812"`) or the 128-bit form
- `ble.connect(scanResult)` — accepts the request and returns immediately; the connection itself runs on an internal task
  - `ble.connect(scanResult, timeoutMilliseconds)` — the timeout is enforced from `update()` (default 10000 ms)
- `ble.connect(address, EspBleAddressType, timeoutMilliseconds)` — connect from a saved address without scanning
- `ble.onConnected(callback)` / `ble.onDisconnected(callback)` — both carry the same stable library `connection.id`
- `ble.onConnectionFailed(callback)` — asynchronous failure with `failure.detail`

## Expected Serial output

```
Connected to 5a:b8:1e:0c:2f:71 (id=1)
Disconnected (id=1)
```

## Related guides

- [BLE guide §2 GAP](../../../docs/GUIDE_BLE_BASICS.md#2-gap--finding-and-connecting) — advertising, scanning and connecting

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [Connect.ino](Connect.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

Connect - scan for a peripheral advertising a specific service UUID and connect. Shows the async model: connect() only accepts the request; completion/failure arrives later as an event delivered from update().

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
4. `ble.onConnectionFailed(...)` — Observe failures that happen after a connection request was accepted, including an unavailable peer or a rejected connection.
5. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
6. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Choose a peer using the result filter before connecting. Compare requested, connected and failed events separately; this example stops at a GAP connection rather than exchanging application GATT values.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
