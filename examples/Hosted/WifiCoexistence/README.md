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

## Related guides

- [ESP-Hosted setup (Japanese)](../../../docs/ESP_HOSTED_SETUP.ja.md) — wiring, versions and the C6 firmware
- [Verified ESP-Hosted limitations (Japanese)](../../../docs/ESP_HOSTED_LIMITATIONS.ja.md) — what does not work yet, and why

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [WifiCoexistence.ino](WifiCoexistence.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

Wi-Fi and BLE at the same time on ESP32-P4, which has no radio of its own and reaches both through an ESP32-C6 over ESP-Hosted. The two share one transport, so the order of starting and stopping them matters in a way it does not on a SoC with a built-in controller.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.
2. `ble.scanner().onResult(...)` — Inspect the discovery result before selecting a peer. A name may be absent, and seeing a device does not prove it supports the intended connected service.

## Verify and adapt

Configure the Wi-Fi credentials locally, then observe BLE scanning while Wi-Fi stays connected. Test stopping BLE alone before stopping both users of the shared transport; follow the Hosted setup guide for the radio-side firmware.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
