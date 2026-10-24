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

## Related guides

- [Classic guide §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.md#6-hid) — the SDP record, the 214-byte budget and what a Host decodes
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — report IDs on Classic, and how to verify a descriptor
- [BLE or Bluetooth Classic](../../../docs/CLASSIC_VS_BLE.md) — how Classic HID differs from BLE HID

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [HidVendorDevice.ino](HidVendorDevice.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [HidVendorHost](../HidVendorHost/README.md). For a two-board test, prepare the matching Classic profile on the peer; make the accepting device discoverable when needed and configure the initiating side with its address.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `bluetooth.hidDevice().onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
2. `bluetooth.hidDevice().onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
3. `bluetooth.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. Configure this Classic profile in the order shown by the sketch.

## Verify and adapt

Pair with a compatible host, focus a harmless test window, then send the Serial commands shown above. Key/button presses carry state: release held inputs explicitly and keep report ID, payload length and descriptor consistent. A remembered host may cache the old descriptor after you change the sketch; remove its pairing and reconnect when testing a changed layout.

A vendor-defined report needs a matching application/parser on the other end. Changing bytes in the descriptor requires updating report declarations and both encoders/decoders; do not infer the layout from a friendly device name.

Use the original ESP32 and a peer supporting this Classic profile. BLE-only hardware or a BLE-only phone application cannot substitute for a Classic SPP, HID or audio peer; use the root support information when selecting hardware.

Keep calling `bluetooth.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
