# ConsumerControl

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 6, "HID"

A BLE HID Consumer Control device over GATT (HOGP) — media keys for volume and playback. Advertises the HID service `0x1812`; keys are triggered by Serial commands.

## Hardware

- 1 × ESP32-S3 running this sketch (HID device / peripheral)
- 1 × HID host: a PC or phone, or a second board running [KeyboardHost](../KeyboardHost/) (its `onConsumerControl()` handler prints the events)

## What it does

- Calls `ble.hidConsumerControl().configure()` before `begin()` to compose the HID service
- Enables security with bonding — HOGP requires an encrypted link
- Restarts advertising on each disconnect
- Send `+` for Volume Up, `-` for Volume Down, `p` for Play/Pause

## Key APIs

- `ble.hidConsumerControl().configure()` — compose the HID consumer-control service before `begin()`
- `consumer.click(usage)` — press and release a consumer usage, e.g. `ESP_BLE_HID_CONSUMER_CONTROL_VOLUME_UP`, `..._VOLUME_DOWN`, `..._PLAY_PAUSE`

## Expected Serial output

```
Send '+', '-', or 'p'.
```

## Related guides

- [BLE guide §6 HID](../../../docs/GUIDE_BLE_BASICS.md#6-hid--acting-as-a-keyboard-or-a-mouse) — reports, descriptors and what a Host expects
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — when you need your own descriptor, and how to verify it

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [ConsumerControl.ino](ConsumerControl.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.hidConsumerControl().configure(...)` — Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
2. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
3. `ble.onDisconnected(...)` — Clear readiness and any saved connection/session state here; old IDs must not be reused for a new peer. Restart scanning or advertising only where the sketch explicitly does so.

## Verify and adapt

Pair with a compatible host, focus a harmless test window, then send the Serial commands shown above. Key/button presses carry state: release held inputs explicitly and keep report ID, payload length and descriptor consistent. A remembered host may cache the old descriptor after you change the sketch; remove its pairing and reconnect when testing a changed layout.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
