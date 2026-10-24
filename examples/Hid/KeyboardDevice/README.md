# KeyboardDevice

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 6, "HID"

Turns the board into a BLE HID keyboard over GATT (HOGP, fixed 6-key rollover) that advertises the HID service `0x1812`. Paired from a PC or phone it types like a real keyboard; keystrokes are triggered by Serial commands, and it also receives the host's LED output report. HID Boot Protocol is enabled so BIOS-class hosts are supported.

## Hardware

- 1 × ESP32-S3 running this sketch (HID device / peripheral)
- 1 × HID host: a PC or phone, or a second board running [KeyboardHost](../KeyboardHost/)

## What it does

- Configures the HID Keyboard profile before `begin()` (HID + Device Information + Battery services are composed automatically, and the HID UUID / keyboard appearance are added to advertising)
- Opts into Boot Protocol so `onProtocolMode()` is meaningful and boot hosts receive the 8-byte boot report automatically
- Enables security with bonding — HOGP requires an encrypted link
- Prints the host's LED state (Num/Caps/Scroll Lock) via `onOutputReport()`, and the current Protocol Mode via `onProtocolMode()`
- Restarts advertising on each disconnect
- Send `a` to press Shift+A, `r` to release all keys

## Key APIs

- `ble.hidKeyboard()` / `keyboard.configure(config)` — configure before `begin()`; `EspBleHidKeyboardConfig` sets `manufacturer` and opts into `bootProtocol`
- `EspBleHidKeyboardInputReport` — `modifiers` bitmask (e.g. `LeftShift`) plus up to 6 HID usages in `keys[]`
- `keyboard.sendReport(report)` / `keyboard.releaseAll()`
- `keyboard.onOutputReport(cb)` — LED state (`numLock`, `capsLock`, `scrollLock`)
- `keyboard.ledState()` — ask for the current LED state instead of reacting to a change
- `keyboard.onProtocolMode(cb)` — compare against `EspBleHidKeyboard::BootProtocolMode`

## Notes

- The report ID and reserved byte in the input report are handled internally.
- Output-report and protocol-mode events are delivered from `ble.update()`.

## Expected Serial output

```
Send 'a' to type Shift+A and 'r' to release all keys.
Protocol Mode: Report
Keyboard LEDs: num=0 caps=1 scroll=0
```

## Related guides

- [BLE guide §6 HID](../../../docs/GUIDE_BLE_BASICS.md#6-hid--acting-as-a-keyboard-or-a-mouse) — reports, descriptors and what a Host expects
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — when you need your own descriptor, and how to verify it

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [KeyboardDevice.ino](KeyboardDevice.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [KeyboardHost](../KeyboardHost/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## How this example is structured

KeyboardDevice - turn the board into a BLE HID keyboard (HID over GATT, fixed 6KRO). Paired from a PC/phone it types like a real keyboard; keystrokes are triggered by Serial commands. Also receives the host's LED output report (Num/Caps/Scroll Lock).

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `keyboard.configure(...)` — Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
2. `keyboard.onOutputReport(...)` — Observe a host-to-device HID output report. Check its report ID and length before interpreting fields such as keyboard LED bits.
3. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
4. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.

## Verify and adapt

Pair with a compatible host, focus a harmless test window, then send the Serial commands shown above. Key/button presses carry state: release held inputs explicitly and keep report ID, payload length and descriptor consistent. A remembered host may cache the old descriptor after you change the sketch; remove its pairing and reconnect when testing a changed layout.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
