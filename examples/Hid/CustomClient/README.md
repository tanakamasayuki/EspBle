# CustomClient

> 日本語版: [README.ja.md](README.ja.md)

Reads a Custom HID device's arbitrary Report Descriptor and receives its input reports using the **generic GATT client**. Pairs with the [Hid/CustomDevice](../CustomDevice/) example.

A HID device exposes several Report characteristics that all share UUID `0x2A4D`. The generic GATT client addresses characteristics by UUID, so this example subscribes to the first Report characteristic — the CustomDevice example registers its input report first. (For full control over individual reports by handle, use `ble.hidHost()` instead.)

## Hardware

- 1 × ESP32-S3 running this sketch (central / GATT client)
- 1 × ESP32-S3 running Hid/CustomDevice (HID device / peripheral)

## What it does

- Scans for and connects to a device advertising the HID service (`0x1812`)
- Reads the Report Map (`0x2A4B`) and prints its length
- Subscribes to the input Report (`0x2A4D`) and decodes the 2-byte report (dial delta + buttons)

## Notes

- The CustomDevice example runs with security enabled; a client without bonding may be rejected. Disable security on the device (or add bonding to this client) for a plain unencrypted demo.
