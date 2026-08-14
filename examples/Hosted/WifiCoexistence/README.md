# WifiCoexistence

> 日本語版: [README.ja.md](README.ja.md)
> Setup: [ESP-Hosted setup](../../../docs/ESP_HOSTED_SETUP.ja.md)

Wi-Fi and BLE at the same time on ESP32-P4, which has no radio of its own and
reaches both through an ESP32-C6 over ESP-Hosted. The two share one transport, so
the order of starting and stopping them matters in a way it does not on a SoC
with a built-in controller.

On a target that is not ESP-Hosted the sketch compiles and says so rather than
failing to build — there is nothing to demonstrate there.

## Hardware

- 1 × ESP32-P4 with an ESP32-C6 over SDIO, running this sketch
- 1 × Wi-Fi network, and any BLE device in range to be found by the scan

Set `WIFI_SSID` and `WIFI_PASSWORD`, or pass them as compiler defines.

## What it does

- Starts Wi-Fi first, which also brings the shared transport up; either order
  works, and neither owns the transport exclusively
- Scans for BLE devices while Wi-Fi carries traffic, printing whether Wi-Fi is
  still connected with each result
- `ble.end()` releases what BLE owns and leaves Wi-Fi and the transport running
- The transport is freed only when its last user goes away, which is why stopping
  Wi-Fi last is what finally releases it

## Serial commands

| Key | Effect |
|---|---|
| `b` | stop BLE only; Wi-Fi keeps running |
| `w` | stop BLE and Wi-Fi, releasing the transport |
