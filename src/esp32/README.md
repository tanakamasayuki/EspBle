# Custom Classic Bluedroid host

`libespble_bluedroid_classic.a` is built from ESP-IDF v5.5.5 with its
controller and BLE disabled, and with Classic SPP, HID Device, HID Host, A2DP
Sink/Source and AVRCP CT/TG (external codec), and HFP Client/Audio Gateway
(Voice over HCI with an external codec) enabled. All symbols defined by the archive are prefixed with `espble_bd_` so
Arduino-ESP32's built-in Bluedroid host cannot satisfy or collide with them.
Undefined platform dependencies such as FreeRTOS, NVS and logging keep their
original names and are supplied by Arduino-ESP32.

To regenerate it, install and export ESP-IDF v5.5.5, then run:

```sh
tools/build_classic_bluedroid_host.sh
```

The output is ABI-bound to ESP-IDF v5.5.5 and xtensa-esp32 GCC 14.2.0, matching
Arduino-ESP32 3.3.11. The build script rejects a different IDF tag, a dirty IDF
checkout, or a different compiler instead of silently producing an incompatible
library. It also checks the required HCI, SPP, HID, A2DP, AVRCP and HFP symbols and prints the archive
size, global-symbol count and SHA-256 digest.

The complete clean-room setup, reproducibility check, configuration and update
procedure are documented in
[`docs/CLASSIC_HOST_BUILD.ja.md`](../../docs/CLASSIC_HOST_BUILD.ja.md).
