# CustomDevice

> 日本語版: [README.ja.md](README.ja.md)

A BLE HID device over GATT (HOGP) built with an **arbitrary Report Descriptor** via `ble.hidCustom()`. You supply the raw HID Report Map and declare each report yourself, so any device shape is possible; custom reports compose into the same HID service (`0x1812`) and can coexist with `hidKeyboard()`/`hidMouse()`. This example is a vendor-defined "control panel" (vendor usage page `0xFF00`, Report ID 1): a 2-byte input report (a signed dial delta + a button bitfield) and a 1-byte output report (an LED state written by the host).

## Hardware

- 1 × ESP32-S3 running this sketch (HID device / peripheral)
- 1 × HID host: a second board running [CustomClient](../CustomClient/), or a GATT client app (e.g. nRF Connect)

## What it does

- Calls `configure()`, `addInputReport(1, 2)`, `addOutputReport(1, 1)`, then `setReportMap()` with the raw descriptor before `begin()`
- Advertises the HID service (`0x1812`)
- Enables security with bonding — real hosts require an encrypted link for HID
- Prints the 1-byte output report whenever the host writes it, via `onOutputReport()`
- Restarts advertising on each disconnect
- Send `i` to send a 2-byte input report (dial delta `+5`, buttons `0x01`)

## Key APIs

- `ble.hidCustom().configure()` — must be called before the other `hidCustom()` setup calls, and all before `begin()`
- `custom.addInputReport(id, bytes)` / `addOutputReport(id, bytes)` (also `addFeatureReport`) — declare each report's ID and byte size (1..64)
- `custom.setReportMap(descriptor, size)` — set the raw HID Report Map
- `custom.sendInput(reportId, data, length)` — send an input report matching the declared size
- `custom.onOutputReport(cb)` — receive host writes as `EspBleHidVendorReport` (`reportId`, `length`, `data`)

## Notes

- Report IDs must be unique; Report IDs 1..6 are reserved only when the matching built-in profile is also enabled.
- Output/feature events are delivered from `ble.update()`.

## Expected Serial output

```
Send 'i' to send an input report (dial +5, button 1).
Output report id=1 len=1 value=2
```
