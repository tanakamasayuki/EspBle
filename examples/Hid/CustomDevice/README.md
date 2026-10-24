# CustomDevice

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 6, "HID"

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

## Related guides

- [BLE guide §6 HID](../../../docs/GUIDE_BLE_BASICS.md#6-hid--acting-as-a-keyboard-or-a-mouse) — reports, descriptors and what a Host expects
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — when you need your own descriptor, and how to verify it

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [CustomDevice.ino](CustomDevice.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [CustomClient](../CustomClient/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

CustomDevice - build a HID device with an ARBITRARY Report Descriptor using ble.hidCustom(). This example is a vendor-defined "control panel": a 2-byte input report (a signed dial delta + a button bitfield) and a 1-byte output report (an LED state written by the host). Custom reports are composed into the same HID service, so they can coexist with ble.hidKeyboard()/hidMouse().

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `custom.configure(...)` — Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
2. `custom.onOutputReport(...)` — Observe a host-to-device HID output report. Check its report ID and length before interpreting fields such as keyboard LED bits.
3. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
4. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.

## Verify and adapt

Pair with a compatible host, focus a harmless test window, then send the Serial commands shown above. Key/button presses carry state: release held inputs explicitly and keep report ID, payload length and descriptor consistent. A remembered host may cache the old descriptor after you change the sketch; remove its pairing and reconnect when testing a changed layout.

A vendor-defined report needs a matching application/parser on the other end. Changing bytes in the descriptor requires updating report declarations and both encoders/decoders; do not infer the layout from a friendly device name.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
