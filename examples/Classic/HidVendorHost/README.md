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

## Related guides

- [Classic guide §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.md#6-hid) — the SDP record, the 214-byte budget and what a Host decodes
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — report IDs on Classic, and how to verify a descriptor
- [BLE or Bluetooth Classic](../../../docs/CLASSIC_VS_BLE.md) — how Classic HID differs from BLE HID

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HidVendorHost.ino](HidVendorHost.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [HidVendorDevice](../HidVendorDevice/README.md). For a two-board test, prepare the matching Classic profile on the peer; make the accepting device discoverable when needed and configure the initiating side with its address.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.hidHost().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
2. `bluetooth.hidHost().onConnectionFailed(...)` — Observe failures that happen after a connection request was accepted, including an unavailable peer or a rejected connection.
3. `bluetooth.hidHost().onInputReport(...)` — Inspect an incoming HID report using its descriptor, report ID and length; vendor data does not automatically become a keyboard event.
4. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.
5. `bluetooth.hidHost().connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Use a matching HID device and wait for profile discovery/readiness, not just the radio connection. Route each report according to its ID and descriptor; sending keyboard LED/output reports is the host-to-device direction, independent of receiving key input.

A vendor-defined report needs a matching application/parser on the other end. Changing bytes in the descriptor requires updating report declarations and both encoders/decoders; do not infer the layout from a friendly device name.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
