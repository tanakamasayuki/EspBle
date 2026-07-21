# CustomClient

> 日本語版: [README.ja.md](README.ja.md)

Reads a Custom HID device's arbitrary Report Descriptor and drives its reports using the **generic GATT client**. Pairs with the [Hid/CustomDevice](../CustomDevice/) example.

A HID device exposes several Report characteristics that all share UUID `0x2A4D`. To target a specific one, this example discovers the service, resolves each report to its distinct **attribute handle** with `discoveredCharacteristic()`, then subscribes to the input report and writes the output report **by handle** (`subscribe(connectionId, handle, …)` / `writeCharacteristic(connectionId, handle, …)`). Incoming notifications carry the source `handle`, so the input report is matched unambiguously.

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × ESP32-S3 running Hid/CustomDevice (HID device / peripheral)

## What it does

- Scans for and connects to a device advertising the HID service (`0x1812`)
- Discovers services and resolves the input and output Report characteristics to their handles
- Subscribes to the input report by handle and decodes the 2-byte report (dial delta + buttons)
- Writes the 1-byte output report by handle when you send `o` over Serial

## Notes

- The CustomDevice example runs with security enabled; a client without bonding may be rejected. Disable security on the device (or add bonding to this client) for a plain unencrypted demo.
