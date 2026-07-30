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
