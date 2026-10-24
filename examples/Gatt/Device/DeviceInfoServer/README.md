# DeviceInfoServer

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../../docs/GUIDE_BLE_BASICS.md) — chapter 4, "GATT"

Peripheral publishing the standard Device Information Service (`0x180A`) with Manufacturer Name (`0x2A29`), Model Number (`0x2A24`), Firmware Revision (`0x2A26`), and PnP ID (`0x2A50`).

## Hardware

- 1 × ESP32-S3 running this sketch (peripheral)
- 1 × central: the [DeviceInfoClient](../DeviceInfoClient/) example, or any BLE central/phone

## What it does

- Registers four read-only characteristics before `begin()` and advertises `0x180A`
- Sets Manufacturer = "EspBle", Model = "DeviceInfoServer", Firmware = the library version string
- Encodes PnP ID in the standard 7-byte layout: Vendor ID Source `0x02` (USB-IF) + little-endian Vendor ID `0x1234`, Product ID `0x5678`, Product Version `0x0100`

## Key APIs

- `ble.gattServer().addCharacteristic(..., { .readable = true })` — declare each read-only characteristic
- `ble.gattServer().setValue(...)` — set string and raw-byte values

## Notes

- PnP ID is a fixed 7-byte binary; multibyte fields are little-endian on the wire.

## Expected Serial output

```
Device Information Service is ready.
```

## Related guides

- [BLE guide §4 GATT](../../../../docs/GUIDE_BLE_BASICS.md#4-gatt--exchanging-data) — services, characteristics, notify and MTU
- [BLE guide §5 UUIDs](../../../../docs/GUIDE_BLE_BASICS.md#5-understanding-uuids) — 16-bit and 128-bit forms

## Before running this sketch

See the [example workflow](../../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [DeviceInfoServer.ino](DeviceInfoServer.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [DeviceInfoClient](../DeviceInfoClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `server.addService(...)` — Create the local service before BLE begin; the service UUID identifies a collection of characteristics, not a specific device.
2. `server.addCharacteristic(...)` — Declare the value and allowed operations before BLE begin. Read, write, notify and indicate are separate capabilities.
3. `server.setValue(...)` — Change the locally stored characteristic value. This alone does not send a notification or indication to a subscribed peer.
4. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
5. `ble.advertising().addServiceUuid(...)` — Include the service in advertising so a service-filtered scanner can find it; advertising a UUID does not create the GATT service itself.

## Verify and adapt

Device Information contains descriptive values such as manufacturer and model. Keep text and binary identifiers distinct; these values do not authenticate the device that advertised them.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../../README.md) · [Example workflow](../../../../docs/GUIDE_EXAMPLES.md)
