# CustomClient

> 日本語版: [README.ja.md](README.ja.md)

Reads a Custom HID device's arbitrary Report Descriptor and drives its reports using the **generic GATT client** (central). Pairs with the [CustomDevice](../CustomDevice/) example. A HID device exposes several Report characteristics that all share UUID `0x2A4D`, so this example discovers the HID service (`0x1812`), resolves each report to its distinct **attribute handle**, subscribes to the input report by handle, and writes the output report by handle.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × ESP32-S3 running [CustomDevice](../CustomDevice/) (HID device / peripheral)

## What it does

- Actively scans and connects to a device advertising the HID service (`0x1812`)
- On connect, discovers services; when done, walks the HID service's characteristics and resolves the notifiable one to `inputHandle` and the writable one to `outputHandle`
- Subscribes to the input report by handle and decodes the 2-byte report (signed dial delta + buttons)
- Send `o` to write a 1-byte output report (`0x02`, LED state) by handle

## Key APIs

- `ble.discoverServices(connectionId)` / `ble.onServicesDiscovered(cb)` — trigger and receive GATT discovery
- `ble.discoveredCharacteristicCount(connectionId, serviceUuid)` / `ble.discoveredCharacteristic(connectionId, index, info, serviceUuid)` — enumerate characteristics; `EspBleGattCharacteristicInfo` carries `characteristicUuid`, `handle`, `notifiable`, `writable`
- `ble.subscribe(connectionId, handle, true)` — subscribe by attribute handle
- `ble.onNotification(cb)` — `EspBleGattNotification` with the source `handle` and `value`
- `ble.writeCharacteristic(connectionId, handle, data, length, response)` — write by handle

## Notes

- CustomDevice runs with security enabled, so a client without bonding may be rejected. Disable security on the device (or add bonding here) for a plain unencrypted demo.
- Discovered UUIDs come back in 128-bit form (`0000XXXX-...`); the sketch matches the 16-bit short form either way.

## Expected Serial output

```
Scanning for a Custom HID device. Send 'o' to write the output LED report.
Resolved input handle=42, output handle=45
Input report: dial delta=5 buttons=1
```
