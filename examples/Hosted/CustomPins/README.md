# Hosted / CustomPins

> 日本語版: [README.ja.md](README.ja.md)

Overrides the SDIO pins between an ESP32-P4 and its ESP-Hosted coprocessor at
runtime through Arduino Core's `hostedSetPins()`. The values in the sketch are
the M5Stack Tab5 wiring.

Selecting the matching board in Arduino IDE is normally preferred. Building a
Tab5 as `M5Tab5` (`esp32:esp32:m5stack_tab5` with the CLI) automatically uses
the pins from the Core's `variants/m5stack_tab5/pins_arduino.h`, so no runtime
override is needed.

Use this example for custom P4/C6 hardware, a board whose variant is not yet in
the installed Core, or hardware whose wiring differs from the generic P4 board.

## Required order

Call `hostedSetPins()` before the shared ESP-Hosted transport is initialized:

```cpp
hostedSetPins(clk, cmd, d0, d1, d2, d3, reset);
ble.begin();
```

If Wi-Fi starts first, configure the pins before `WiFi.STA.begin()`. Arduino
Core rejects changes after initialization, negative pins, and partial pin sets.
The override is held only in RAM; a reboot restores the board variant values.

```sh
# Build as generic P4 and override the wiring in the sketch.
arduino-cli compile --profile esp32p4 examples/Hosted/CustomPins

# Also verify the Tab5 board variant itself.
arduino-cli compile --profile m5stack_tab5 examples/Hosted/CustomPins
```

The pins belong to the SDIO transport shared by Wi-Fi and BLE, so this example
uses Arduino Core's Hosted HAL instead of defining an EspBle-specific API. See
the Japanese [ESP-Hosted setup guide](../../../docs/ESP_HOSTED_SETUP.ja.md#sdio-pinの選択と上書き)
for the complete pin table and troubleshooting guidance.

## Related guides

- [ESP-Hosted setup (Japanese)](../../../docs/ESP_HOSTED_SETUP.ja.md) — wiring, versions and the C6 firmware
- [Verified ESP-Hosted limitations (Japanese)](../../../docs/ESP_HOSTED_LIMITATIONS.ja.md) — what does not work yet, and why

## Before running this sketch

See the [example workflow](../../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [CustomPins.ino](CustomPins.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. Use the command table above or the sketch startup message for input keys.

## How this example is structured

ESP32-P4 ESP-Hosted custom SDIO pins. Prefer selecting the correct Arduino board definition. Use this override only for custom hardware or when its board variant is not available.

## Follow the code and its APIs

The entries below follow their first appearance in the source. Registering a callback does not run it; the matching event invokes it later.

1. `ble.begin(...)` — Initialize the selected stack/profile and check its return value before starting dependent work. For BLE GATT servers, register the database before stack initialization.

## Verify and adapt

Apply SDIO pins before BLE or Wi-Fi initializes the shared Hosted transport. Replace the entire board-specific pin set together; the sketch is not a way to add Bluetooth hardware to a board with no radio peer.

Keep calling `ble.update()` from loop. Long waits for Serial input or periodic work postpone queued events and subsequent notifications.

When troubleshooting, locate the last completed stage: initialization, discovery/advertising, connection, required security, discovery/subscription, then value exchange. Skip stages this example does not use; a quiet server is verified through peer reads or received updates. For bool-returning requests, distinguish immediate acceptance from later completion events. When adapting a minimal example that does not print every result, add return-value checks and a failure path.

[Example index](../../README.md) · [Example workflow](../../../docs/GUIDE_EXAMPLES.md)
