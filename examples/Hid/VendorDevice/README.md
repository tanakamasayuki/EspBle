# VendorDevice

> 日本語版: [README.ja.md](README.ja.md)

A BLE HID device over GATT (HOGP) exposing a vendor-defined report (fixed Report ID 6) for arbitrary Input / Output / Feature data. Advertises the HID service `0x1812`; Input is sent on a Serial command, and host writes are printed. The report size is configurable from 1 to 64 bytes (8 here).

## Hardware

- 1 × ESP32-S3 running this sketch (HID device / peripheral)
- 1 × HID host: a second board running [VendorHost](../VendorHost/)

## What it does

- Configures the vendor HID profile with `EspBleHidVendorConfig::reportSize = 8` before `begin()`
- Requests a preferred MTU of 100
- Enables security with bonding — HOGP requires an encrypted link
- Prints Output and Feature reports written by the host via `onOutputReport()` / `onFeatureReport()`
- Restarts advertising on each disconnect
- Send `i` to send an 8-byte Vendor Input report (`'E' 'S' 'P'` + a rolling counter + `4 5 6 7`)

## Key APIs

- `ble.hidVendor().configure(vendorConfig)` — `EspBleHidVendorConfig` sets `reportSize`; returns false on failure
- `vendor.sendInput(data, length)` — send a Vendor Input report (returns success)
- `vendor.onOutputReport(cb)` / `vendor.onFeatureReport(cb)` — receive host writes as `EspBleHidVendorReport` (`reportType`, `length`, `data`)

## Notes

- The report layout and Report ID 6 align with EspUsbDevice Vendor HID.
- Output/Feature events are delivered from `ble.update()`.

## Expected Serial output

```
Send 'i' to send an 8-byte Vendor Input Report.
Input: sent
Output type=2 length=8 data=4f 55 54 03 04 05 06 07
```
