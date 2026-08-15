# HidConsumerControl (Classic)

> 日本語版: [README.ja.md](README.ja.md)

Media keys and system requests over Bluetooth Classic (BR/EDR). Car audio units
and older TVs accept Classic HID, so this is the transport for them. Mirrors
[Hid/ConsumerControl](../../Hid/ConsumerControl/) on the BLE side. **Classic
works on the original ESP32 only.**

## Hardware

- 1 × original ESP32 running this sketch
- 1 × Classic HID host that acts on media keys: a car audio unit, a TV, or a PC

## What it does

- Configures Consumer Control and System Control, which are separate profiles
  because a Host treats power and sleep differently from media keys
- Sends volume, play/pause, next track and a sleep request from Serial commands

## Key APIs

- `bluetooth.hidConsumerControl().click(usage)` — press and release; a Host acts
  on the press and needs the release to stop repeating
- `bluetooth.hidSystemControl().click(usage)` — power, sleep and wake requests

## Serial commands

| Key | Effect |
|---|---|
| `+` / `-` | volume up / down |
| `p` | play or pause |
| `n` | next track |
| `s` | sleep request (Generic Desktop usage 0x82) |

## Related guides

- [Classic guide §6 HID](../../../docs/GUIDE_CLASSIC_BASICS.md#6-hid) — the SDP record, the 214-byte budget and what a Host decodes
- [Writing a HID Report Descriptor](../../../docs/GUIDE_HID_DESCRIPTORS.md) — report IDs on Classic, and how to verify a descriptor
- [BLE or Bluetooth Classic](../../../docs/CLASSIC_VS_BLE.md) — how Classic HID differs from BLE HID
