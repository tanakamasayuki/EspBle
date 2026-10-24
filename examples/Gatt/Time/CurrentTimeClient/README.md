# CurrentTimeClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Connects to a Current Time Service (0x1805), reads and decodes the 10-byte Current Time (0x2A2B), then subscribes to notifications and decodes each update.

## Hardware

- 1 × ESP32-S3 running this sketch (central)
- 1 × Current Time peripheral: the [CurrentTimeServer](../CurrentTimeServer/) example

## What it does

- Active-scans for and connects to a device advertising 0x1805
- Reads Current Time (0x2A2B) and decodes the 10-byte wire format (year, date, time, weekday, fraction, adjust reason)
- Subscribes to Current Time notifications and decodes each change

## Key APIs

- `ble.readCharacteristic(...)` — read the initial Current Time
- `ble.subscribe(...)` — subscribe to Current Time notifications
- `ble.onNotification(...)` — decode each notified update

## Expected Serial output

```
Current Time: 2026-07-19 12:34:56 weekday=7 fraction=0/256 reason=0x01
Current Time subscription: ready
Current Time changed: 2026-07-19 12:34:57 weekday=7 fraction=0/256 reason=0x01
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [CurrentTimeClient.ino](CurrentTimeClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [CurrentTimeServer](../CurrentTimeServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.
4. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
5. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
6. `ble.onSubscribed(...)` — Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
7. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
8. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
9. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Current Time is a structured date/time value, not Unix seconds. Keep calendar fields, day of week, fractions and adjustment reason coherent; the sample is not an RTC or a time synchronization source.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
