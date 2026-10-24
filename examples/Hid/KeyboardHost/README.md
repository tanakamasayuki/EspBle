# KeyboardHost

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 6, "HID"

Connects to a BLE keyboard as a HID host (central): scans for the HID service `0x1812`, pairs, discovers the HID reports, and prints key events. The single `hidHost()` object also dispatches mouse, consumer-control, system-control, and gamepad reports, so it works with commercial BLE keyboards and with the [KeyboardDevice](../KeyboardDevice/), [KeyboardNkro](../KeyboardNkro/), or [CompositeKeyboardMouse](../CompositeKeyboardMouse/) example on a second board.

## Hardware

- 1 × ESP32-S3 running this sketch (HID host / central)
- 1 × BLE HID device: a commercial keyboard, or a second board running KeyboardDevice / KeyboardNkro / CompositeKeyboardMouse

## What it does

- Scans and connects to the first connectable device advertising the HID service `1812`
- Enables security with bonding and starts HID discovery **after** `onSecurityChanged` reports success — commercial keyboards reject HID-attribute access before encryption
- Sets the keyboard layout to EN-US, then prints the discovery result (report ID, battery), the raw usage state snapshot, and per-key press events with the layout-converted ASCII value
- Also prints mouse, consumer-control, system-control, and gamepad events from the same `hidHost()` object
- Send `c` to set the Caps Lock LED, `0` to clear all LEDs (only while connected)

## Key APIs

- `ble.hidHost().discover(connectionId)` — explicit HID discovery; re-call with the new connection ID after each reconnect
- `keyboard.onDiscovered(cb)` — `EspBleHidKeyboardHostDiscovery` with `success`, `reportId`, `hasBatteryLevel`, `batteryLevel`, `detail`
- `keyboard.onKeyboardState(cb)` — layout-independent 256-bit usage snapshot (`isDown()`, `wasPressed()`, `wasReleased()`, `modifiers`)
- `keyboard.setKeyboardLayout(EspBleKeyboardLayout::EnUs)` / `keyboard.onKeyboard(cb)` — per-usage press/release events; `ascii` is nonzero only when convertible
- `keyboard.onMouse()` / `onConsumerControl()` / `onSystemControl()` / `onGamepad()` — typed composite-HID events
- `keyboard.setKeyboardLeds(connectionId, num, caps, scroll)` — fire-and-forget LED write (Write Without Response)

## Notes

- Discovery, state, and key events are all delivered from `ble.update()`.

## Expected Serial output

```
Keyboard ready: report=1 battery=73%
Keyboard state: modifiers=0x00 A=1 pressed=1 released=0
Key pressed: usage=0x04 ascii=0x61
```

The state callback runs before the per-usage key events for the same report.
`ascii` follows the modifiers in that report, so the same key reports `0x61`
without Shift and `0x41` with it.

## Related guides

- [BLE guide §6 HID](../../../docs/GUIDE_BLE_BASICS.md#6-hid--acting-as-a-keyboard-or-a-mouse) — reports, descriptors and what a Host expects
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — when you need your own descriptor, and how to verify it

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [KeyboardHost.ino](KeyboardHost.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [KeyboardDevice](../KeyboardDevice/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

KeyboardHost - connect to a BLE keyboard as a HID host: scan the HID service -> pair -> discover -> print key events. Works with commercial BLE keyboards and with the KeyboardDevice example. Also writes the keyboard LEDs.

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

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
