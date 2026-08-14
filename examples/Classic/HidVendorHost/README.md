# HidVendorHost (Classic)

> 日本語版: [README.ja.md](README.ja.md)

The Host side for a Classic HID device whose reports nothing standard describes.
It connects by address and prints every input report as bytes, which is what a
vendor-specific device needs: the Host decoder here understands keyboards and
mice, and anything else arrives raw. **Classic works on the original ESP32 only**,
and it connects to Classic HID devices.

## Hardware

- 1 × original ESP32 running this sketch
- 1 × Classic HID device: a second original ESP32 running
  [HidVendorDevice](../HidVendorDevice/), or any BR/EDR HID device

## What it does

- Connects to the address typed into the serial console
- Prints each input report with its report ID and bytes
- Reports a connection that failed after the attempt was accepted

## Key APIs

- `bluetooth.hidHost().connect(address)` — Classic has no advertisement to filter,
  so an address is required
- `onInputReport()` — the report exactly as the device sent it
- `onConnectionFailed()` — the attempt failed later, asynchronously

## Notes

**The Classic HID Host decodes keyboard and mouse only.** Consumer, system,
gamepad and vendor reports arrive raw at `onInputReport()`, which is why this
example prints bytes. The BLE host decodes more; see
[BLE and Classic](../../../docs/CLASSIC_VS_BLE.md).

A report from a device that declares report IDs carries the ID as its first
payload byte, so `value` and `reportId` say the same thing twice for such a
device.

One HID Host connection at a time.
