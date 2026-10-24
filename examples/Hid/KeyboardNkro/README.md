# KeyboardNkro

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 6, "HID"

A BLE HID keyboard over GATT (HOGP) that uses the 29-byte NKRO (N-key rollover) bitmap input report instead of the fixed 6-key report, allowing any number of keys to be reported simultaneously. Advertises the HID service `0x1812`; keystrokes are triggered by Serial commands.

## Hardware

- 1 × ESP32-S3 running this sketch (HID device / peripheral)
- 1 × HID host: a PC or phone, or a second board running [KeyboardHost](../KeyboardHost/)

## What it does

- Calls `enableNkro()` before `configure()` to switch the keyboard to the 29-byte NKRO bitmap report
- Requests a preferred MTU of 64 (a 29-byte NKRO input report needs MTU ≥ 32)
- Enables security with bonding — HOGP requires an encrypted link
- Restarts advertising on each disconnect
- Send `n` to press eight simultaneous keys as **one report**, `r` to release all

## Key APIs

- `ble.hidKeyboard().enableNkro()` — must be called before `configure()`
- `keyboard.configure()` — compose the HID service before `begin()`
- `keyboard.ready()` — whether a subscribed host can receive a report right now
- `EspBleHidKeyboardNkroReport::press(usage)` — set one HID usage down in the report being built
- `keyboard.sendReport(nkroReport)` — send the whole NKRO state as one notification
- `keyboard.releaseAll()` — clear all keys
- `config.preferredMtu = 64` — negotiate an MTU large enough for the report

## Notes

- The NKRO bitmap report layout matches EspUsbDevice, so the same usages map identically.
- The ordinary `sendReport()` carries `keys[6]` and expresses at most six keys per call even with NKRO enabled. Use the `EspBleHidKeyboardNkroReport` overload to send seven or more at once.
- The bitmap in `EspBleHidKeyboardNkroReport` covers usages `0x00`-`0xDF`. Modifier usages `0xE0`-`0xE7` are routed into `modifiers` by `press()` / `release()`, so callers do not have to distinguish them.

## Expected Serial output

```
Send 'n' for eight simultaneous keys, 'r' to release all.
```

## Related guides

- [BLE guide §6 HID](../../../docs/GUIDE_BLE_BASICS.md#6-hid--acting-as-a-keyboard-or-a-mouse) — reports, descriptors and what a Host expects
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — when you need your own descriptor, and how to verify it

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [KeyboardNkro.ino](KeyboardNkro.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

The matching example is [KeyboardHost](../KeyboardHost/README.md). For a two-board test, start the peer first and ensure its advertisement/service matches this sketch's filter.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `keyboard.configure(...)` — Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
2. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
3. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.

## Verify and adapt

Pair with a compatible host, focus a harmless test window, then send the Serial commands shown above. Key/button presses carry state: release held inputs explicitly and keep report ID, payload length and descriptor consistent. A remembered host may cache the old descriptor after you change the sketch; remove its pairing and reconnect when testing a changed layout.

Enable NKRO before configuring the keyboard and use the NKRO whole-state report when sending more than six keys. For BLE, verify the negotiated MTU is large enough for that report before testing the chord.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
