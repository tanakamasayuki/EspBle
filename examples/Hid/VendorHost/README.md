# VendorHost

> 日本語版: [README.ja.md](README.ja.md)

A BLE HID host (central) for a vendor-defined HID device: scans for the HID service `0x1812`, pairs, discovers, receives Vendor Input reports, and writes Vendor Output / Feature reports. Uses the same post-security `discover()` flow as every other HID report type.

## Hardware

- 1 × ESP32-S3 running this sketch (HID host / central)
- 1 × HID device: a second board running [VendorDevice](../VendorDevice/)

## What it does

- Scans and connects to the first device advertising the HID service `1812`
- Requests a preferred MTU of 100 and enables security with bonding
- Starts HID discovery from `onSecurityChanged` once encryption succeeds
- Prints incoming Vendor Input reports via `onVendorInput()` (report ID, length, raw bytes)
- Send `o` to write an 8-byte Vendor Output report, `f` to write an 8-byte Vendor Feature report (only while connected)

## Key APIs

- `ble.hidHost().discover(connectionId)` — start HID discovery after security completes
- `hidHost().onDiscovered(cb)` — `EspBleHidKeyboardHostDiscovery` with `success` / `detail`
- `hidHost().onVendorInput(cb)` — `EspBleHidVendorInputEvent` with `reportId`, `rawLength`, `rawData`
- `hidHost().sendVendorOutput(connectionId, data, length)` / `sendVendorFeature(connectionId, data, length)` — write to the device (return success)

## Expected Serial output

```
Send 'o' for Output or 'f' for Feature after discovery.
HID discovery: ready
Vendor Input report=6 length=8 data=45 53 50 00 04 05 06 07
Output: sent
```
