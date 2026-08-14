# HidVendorDevice (Classic)

> 日本語版: [README.ja.md](README.ja.md)

A Classic HID device with a Report Descriptor the sketch writes itself, in a vendor
usage page. This is the escape hatch for a device that is not a keyboard, a mouse,
a gamepad or media keys: the descriptor decides the report layout, and the Host
follows it. **Classic works on the original ESP32 only**, and this reaches a
Classic HID Host — an older console or PC. For a Host that speaks BLE, the same
thing is [Hid/CustomDevice](../../Hid/CustomDevice/).

## Hardware

- 1 × original ESP32 running this sketch
- 1 × Classic HID host: a PC, or a second original ESP32 running
  [HidVendorHost](../HidVendorHost/)

## What it does

- Declares four bytes of input under a vendor usage page, with report ID 1
- Sends a report periodically so a Host has something to show
- Reports when a Host connects and disconnects

## Key APIs

- `EspBleClassicHidDeviceConfig::reportDescriptor` /
  `reportDescriptorLength` — the descriptor the Host will read over SDP
- `bluetooth.hidDevice().begin(hidConfig)` — register the device record
- `sendInputReport(reportId, data, length)` — a report matching the descriptor

## Notes

The descriptor and the device strings share one SDP record, and the two together
may total 214 bytes. A larger descriptor is refused by `begin()` with
`ResourceExhausted` rather than starting a device no Host can find.

A generic Host will not interpret vendor usages: the peer side has to be written
to match, which is what [HidVendorHost](../HidVendorHost/) does by printing raw
reports.
