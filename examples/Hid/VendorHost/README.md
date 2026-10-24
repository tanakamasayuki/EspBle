# VendorHost

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 6, "HID"

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

## Related guides

- [BLE guide §6 HID](../../../docs/GUIDE_BLE_BASICS.md#6-hid--acting-as-a-keyboard-or-a-mouse) — reports, descriptors and what a Host expects
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — when you need your own descriptor, and how to verify it

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [VendorHost.ino](VendorHost.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.onConnected(...)` — Confirm the established link here and retain the peer/session information needed by this profile. An accepted connect request is earlier and does not prove that a usable link exists.
3. `ble.onSecurityChanged(...)` — Read the security result and encrypted/authenticated/bonded flags; these describe different properties and must not be treated as interchangeable.
4. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.
5. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.
6. `ble.connect(...)` — Request a connection to the selected peer; wait for the connection event before using a connection ID or starting profile traffic.

## Verify and adapt

Use a matching HID device and wait for profile discovery/readiness, not just the radio connection. Route each report according to its ID and descriptor; sending keyboard LED/output reports is the host-to-device direction, independent of receiving key input.

A vendor-defined report needs a matching application/parser on the other end. Changing bytes in the descriptor requires updating report declarations and both encoders/decoders; do not infer the layout from a friendly device name.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
