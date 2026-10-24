# CustomClient

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 6, "HID"

Reads a Custom HID device's arbitrary Report Descriptor and drives its reports using the **generic GATT client** (central). Pairs with the [CustomDevice](../CustomDevice/) example.

A HID device exposes several Report characteristics that all share UUID `0x2A4D`, so every attribute here is named by its distinct **attribute handle**. Each report's role is read from its own **Report Reference descriptor** (`0x2908`, one byte of report ID plus one byte of type: 1 = Input, 2 = Output, 3 = Feature) — which is how HID declares it. That descriptor is also addressed by handle: every Report Reference is `0x2908` under a `0x2A4D` characteristic, so a service/characteristic/descriptor UUID triple names all of them at once and none of them in particular.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × ESP32-S3 running [CustomDevice](../CustomDevice/) (HID device / peripheral)

## What it does

- Actively scans and connects to a device advertising the HID service (`0x1812`)
- On connect, discovers services; when done, pairs each `0x2A4D` characteristic with its own `0x2908` descriptor. A descriptor belongs to one characteristic, and the link is the owning value handle, reported as `EspBleGattDescriptorInfo::characteristicHandle`
- Reads every Report Reference **by handle**. Calls are queued automatically and run in order, so all of them are issued at once
- Takes the role from the type byte: the Input report is subscribed to by handle, the Output report's handle is kept for writing
- Decodes the 2-byte input report (signed dial delta + buttons)
- Send `o` to write a 1-byte output report (`0x02`, LED state) by handle

## Key APIs

- `ble.discoverServices(connectionId)` / `ble.onServicesDiscovered(cb)` — trigger and receive GATT discovery
- `ble.discoveredCharacteristicCount(connectionId, serviceUuid)` / `ble.discoveredCharacteristic(connectionId, index, info, serviceUuid)` — enumerate characteristics; `EspBleGattCharacteristicInfo` carries `characteristicUuid`, `handle`, `notifiable`, `writable`
- `ble.discoveredDescriptorCount(...)` / `ble.discoveredDescriptor(...)` — enumerate descriptors; `EspBleGattDescriptorInfo` carries `descriptorUuid`, `handle`, and the owning `characteristicHandle`
- `ble.readDescriptor(connectionId, descriptorHandle)` / `ble.onDescriptorRead(cb)` — read a descriptor by attribute handle. In the result, `descriptorHandle` is the descriptor read and `handle` is the characteristic that owns it
- `ble.subscribe(connectionId, handle, true)` — subscribe by attribute handle
- `ble.onNotification(cb)` — `EspBleGattNotification` with the source `handle` and `value`
- `ble.writeCharacteristic(connectionId, handle, data, length, response)` — write by handle

## Notes

- CustomDevice runs with security enabled, so a client without bonding may be rejected. Disable security on the device (or add bonding here) for a plain unencrypted demo.
- Discovered UUIDs come back in 128-bit form (`0000XXXX-...`); the sketch matches the 16-bit short form either way.
- The UUID form `readDescriptor(connectionId, serviceUuid, characteristicUuid, descriptorUuid)` exists as well, and is the right choice when the characteristic's UUID is unique. It cannot be used here: it would match whichever `0x2A4D` came first, which is not necessarily the report you meant.
- Reading the type rather than guessing from the properties matters because both an Output and a Feature report are writable. Properties still say something useful — only an Output report carries Write Without Response, since a Feature report is configuration and is always written with a response — but the type byte is what the device actually declares.

## Expected Serial output

```
Scanning for a Custom HID device. Send 'o' to write the output LED report.
Reading 2 Report Reference descriptors
Input report: id=1 handle=42
Output report: id=1 handle=45
Input report: dial delta=5 buttons=1
```

## Related guides

- [BLE guide §6 HID](../../../docs/GUIDE_BLE_BASICS.md#6-hid--acting-as-a-keyboard-or-a-mouse) — reports, descriptors and what a Host expects
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — when you need your own descriptor, and how to verify it

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [CustomClient.ino](CustomClient.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [CustomDevice](../CustomDevice/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

CustomClient - read a Custom HID device's arbitrary Report Descriptor and drive its reports using the generic GATT client. Pairs with the Hid/CustomDevice example. A HID device exposes several Report characteristics that share UUID 0x2A4D, so every attribute here is named by its distinct attribute HANDLE. Each report's role comes from its Report Reference descriptor (0x2908, report ID + type), which is read BY HANDLE: every Report Reference is 0x2908 under a 0x2A4D characteristic, so a service/characteristic/descriptor UUID triple cannot pick one out.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.discoverServices(...)` — Start remote database discovery; consume the results in its completion event instead of assuming the request immediately populated handles.
4. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
5. `ble.onServicesDiscovered(...)` — Inspect discovery success and the returned database before using discovered service, characteristic or descriptor handles.
6. `ble.subscribe(...)` — Enable updates for the remote characteristic. Notify and indicate subscriptions differ; use the mode matching the characteristic and wait for subscription completion.
7. `ble.onNotification(...)` — Filter the characteristic and connection, then validate the binary layout before decoding updates. Indication data is also received through this API.
8. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
9. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.
10. `ble.writeCharacteristic(...)` — Queue a remote write and inspect its completion event. Write Without Response provides no ATT response proving the peer application acted on the data.

## Verify and adapt

Use a matching HID device and wait for profile discovery/readiness, not just the radio connection. Route each report according to its ID and descriptor; sending keyboard LED/output reports is the host-to-device direction, independent of receiving key input.

A vendor-defined report needs a matching application/parser on the other end. Changing bytes in the descriptor requires updating report declarations and both encoders/decoders; do not infer the layout from a friendly device name.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
