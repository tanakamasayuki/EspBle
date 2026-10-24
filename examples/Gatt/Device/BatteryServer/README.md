# BatteryServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Peripheral publishing the standard Battery Service (`0x180F`) with a readable and notifiable Battery Level characteristic (`0x2A19`).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [BatteryClient](../BatteryClient/) example, or any BLE central/phone

## What it does

- Registers the Battery Level characteristic (readable + notifiable) before `begin()` and advertises `0x180F`
- Starts at 75% and logs when a client turns notifications on/off
- Send `+` or `-` over Serial to change the level (clamped 0–100); each change updates the stored value and notifies subscribers

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .readable = true, .notifiable = true })` — declare the Battery Level characteristic
- `ble.gattServer().setValue(...)` — store the current level
- `ble.gattServer().notify(...)` — push the level to subscribers (returns whether it was accepted)
- `ble.gattServer().onSubscriptionChanged(...)` — observe when notifications are enabled/disabled

## Expected Serial output

```
Send '+' or '-' to change the Battery Level.
Battery notifications: 1
Battery: 76% (notification accepted: 1)
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [BatteryServer.ino](BatteryServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [BatteryClient](../BatteryClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
2. `server.notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.
3. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
4. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
5. `server.onSubscriptionChanged(...)` — Track which connection subscribed before sending it updates; connecting does not automatically enable characteristic notifications.
6. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
7. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

Keep the Battery Level as a percentage byte, not a raw ADC reading. Replace the demonstration level with a converted measurement and clamp it to the valid percentage range.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
