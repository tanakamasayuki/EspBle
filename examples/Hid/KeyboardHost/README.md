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
