# CustomDevice

> 日本語版: [README.ja.md](README.ja.md)

Builds a HID device with an **arbitrary Report Descriptor** using `ble.hidCustom()`. Unlike the fixed profiles (`hidKeyboard()`, `hidMouse()`, …), you supply the raw HID Report Map and declare each report yourself, so you can expose any device shape. Custom reports are composed into the same HID service, so they can coexist with the built-in profiles.

This example is a vendor-defined "control panel": a 2-byte input report (a signed dial delta + a button bitfield) and a 1-byte output report (an LED state written by the host), both under Report ID 1.

Use the [Hid/CustomClient](../CustomClient/) example on a second board, or any GATT client (e.g. nRF Connect) to read the Report Map and subscribe to the input report.

## Hardware

- 1 × ESP32-S3 running this sketch (HID device / peripheral)
- 1 × HID host (second board running Hid/CustomClient, or a GATT client app)

## What it does

- Calls `ble.hidCustom().configure()`, then `setReportMap()` with the raw descriptor and `addInputReport(1, 2)` / `addOutputReport(1, 1)` before `begin()`
- Advertises the HID service (`0x1812`)
- Sends a 2-byte input report (dial delta `+5`, buttons `0x01`) when you send `i` over Serial
- Prints the 1-byte output report whenever the host writes it, via `onOutputReport()`

## API notes

- `configure()` must be called before `setReportMap()` / `addInputReport()` / `addOutputReport()` / `addFeatureReport()`, and all of them before `begin()`
- Report IDs must be unique; when a built-in profile is also enabled, avoid its reserved report ID (1..6)
- Report sizes are in bytes (1..64); `sendInput(reportId, data, length)` must match the declared size
- A real host normally requires an encrypted link for HID, so security is enabled here
