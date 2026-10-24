# EnvironmentalServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Standard Environmental Sensing Service (0x181A) peripheral. It publishes Temperature (0x2A6E), Humidity (0x2A6F), and Pressure (0x2A6D) using the standard little-endian integer scales: 0.01 °C, 0.01 %, and 0.1 Pa. Temperature is readable and notifiable; humidity and pressure are read-only.

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [EnvironmentalClient](../EnvironmentalClient/) example, or any BLE central/phone

## What it does

- Registers the Environmental Sensing service and advertises 0x181A
- Seeds Temperature 21.50 °C, Humidity 48.75 %, and Pressure 101325.0 Pa as readable values
- Reads Serial: send `+` or `-` to change the temperature by 0.25 °C, then re-publishes and notifies the new value

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .readable = true, .notifiable = true })` — Temperature
- `ble.gattServer().setValue(...)` — update the stored characteristic value
- `ble.gattServer().notify(...)` — notify subscribers; the return flag reports whether it was accepted

## Notes

- `notify()` returns 0 when no client has subscribed to Temperature yet, and 1 once a subscriber is connected.

## Expected Serial output

```
Send '+' or '-' to change temperature by 0.25 C.
Temperature raw: 2175 (notification accepted: 1)
Temperature raw: 2200 (notification accepted: 1)
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [EnvironmentalServer.ino](EnvironmentalServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [EnvironmentalClient](../EnvironmentalClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
2. `server.notify(...)` — Queue an update for subscribers and check acceptance; this does not carry an ATT confirmation from the receiver. Use send results or an application acknowledgement where needed.
3. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
4. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
5. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
6. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

Temperature, humidity and pressure have different integer widths and scales. The sample temperature changes through Serial commands; adding a real sensor means converting its output into the existing wire format.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
