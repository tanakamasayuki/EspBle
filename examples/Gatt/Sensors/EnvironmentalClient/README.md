# EnvironmentalClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Central / GATT client for the Environmental Sensing Service (0x181A). It sequentially reads and decodes Temperature (0x2A6E), Humidity (0x2A6F), and Pressure (0x2A6D), then subscribes to Temperature notifications.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × peripheral: the [EnvironmentalServer](../EnvironmentalServer/) example

## What it does

- Actively scans for a peer advertising 0x181A and connects
- Reads the three characteristics in order (temperature, humidity, pressure), decoding the little-endian values (0.01 °C, 0.01 %, 0.1 Pa)
- After the third read, subscribes to Temperature and prints each subsequent change notification

## Key APIs

- `ble.readCharacteristic(...)` — chained reads driven by `onCharacteristicRead`
- `ble.subscribe(...)` / `ble.onSubscribed(...)` — enable and confirm Temperature notifications
- `ble.onNotification(callback)` — decode changed Temperature values

## Notes

- Temperature and humidity are signed/unsigned 16-bit; pressure is a 32-bit value. Each decode checks the received length before use.

## Expected Serial output

```
Temperature raw: 2150 (0.01 C units)
Humidity raw: 4875 (0.01 % units)
Pressure raw: 1013250 (0.1 Pa units)
Temperature subscription: ready
Temperature changed raw: 2175 (0.01 C units)
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [EnvironmentalClient.ino](EnvironmentalClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [EnvironmentalServer](../EnvironmentalServer/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.readCharacteristic(...)` — Queue a remote value read; the value arrives in onCharacteristicRead, not in the immediate bool return.
2. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
3. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
4. `ble.onCharacteristicRead(...)` — Check success, UUID and value length before decoding. Binary String values can contain zero bytes and must be processed by length.
5. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
6. `ble.onSubscribed(...)` — Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
7. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
8. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
9. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Temperature, humidity and pressure have different integer widths and scales. The sample temperature changes through Serial commands; adding a real sensor means converting its output into the existing wire format.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
