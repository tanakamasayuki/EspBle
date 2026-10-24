# VendorDevice

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 6, "HID"

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

## Related guides

- [BLE guide §6 HID](../../../docs/GUIDE_BLE_BASICS.md#6-hid--acting-as-a-keyboard-or-a-mouse) — reports, descriptors and what a Host expects
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — when you need your own descriptor, and how to verify it

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [VendorDevice.ino](VendorDevice.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.hidVendor().configure(...)` — Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
2. `ble.hidVendor().onOutputReport(...)` — Observe a host-to-device HID output report. Check its report ID and length before interpreting fields such as keyboard LED bits.
3. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
4. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.

## Verify and adapt

Pair with a compatible host, focus a harmless test window, then send the Serial commands shown above. Key/button presses carry state: release held inputs explicitly and keep report ID, payload length and descriptor consistent. A remembered host may cache the old descriptor after you change the sketch; remove its pairing and reconnect when testing a changed layout.

A vendor-defined report needs a matching application/parser on the other end. Changing bytes in the descriptor requires updating report declarations and both encoders/decoders; do not infer the layout from a friendly device name.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
