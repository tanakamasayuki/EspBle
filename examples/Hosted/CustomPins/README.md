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
