# Gamepad

> 日本語版: [README.ja.md](README.ja.md)
> Concepts: [BLE communication beginner guide](../../../docs/GUIDE_BLE_BASICS.md) — chapter 6, "HID"

A BLE HID gamepad over GATT (HOGP): six signed axes, a hat switch and 32
buttons in one Input Report. The same calls exist on the Classic side
([Classic/HidGamepad](../../Classic/HidGamepad/)), which is what an older
console or PC needs — those accept BR/EDR HID only.

## Hardware

- 1 × ESP32-S3 running this sketch (HID device / peripheral)
- 1 × HID host: a PC or phone, or a second board running
  [KeyboardHost](../KeyboardHost/) (its `onGamepad()` handler prints the events)

## What it does

- Calls `ble.hidGamepad().configure()` before `begin()` to compose the HID
  service
- Enables security with bonding — HOGP requires an encrypted link
- Restarts advertising on each disconnect
- Sends buttons, a stick position and a hat direction from Serial commands

## Key APIs

- `ble.hidGamepad().configure()` — compose the gamepad into the HID service
- `send(x, y, z, rz, rx, ry, hat, buttons)` — one report from its axes
- `sendReport(report)` — the same thing from a filled `EspBleHidGamepadReport`
- `releaseAll()` — everything centred and released; a Host holds the last report
  it received until then

## Serial commands

| Key | Effect |
|---|---|
| `a` | button 1 |
| `b` | button 2 |
| `d` | left stick up-right, hat up, button 1 |
| `r` | release everything |

## Related guides

- [BLE guide §6 HID](../../../docs/GUIDE_BLE_BASICS.md#6-hid--acting-as-a-keyboard-or-a-mouse) — reports, descriptors and what a Host expects
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — when you need your own descriptor, and how to verify it
