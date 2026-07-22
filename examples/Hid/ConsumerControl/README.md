# ConsumerControl

> 日本語版: [README.ja.md](README.ja.md)

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
