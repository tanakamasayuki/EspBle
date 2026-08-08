# Custom Classic Bluedroid host

`libespble_bluedroid_classic.a` is built from ESP-IDF v5.5.5 with its
controller and BLE disabled, and with Classic SPP, HID Device and HID Host
enabled. All symbols defined by the archive are prefixed with `espble_bd_` so
Arduino-ESP32's built-in Bluedroid host cannot satisfy or collide with them.
Undefined platform dependencies such as FreeRTOS, NVS and logging keep their
original names and are supplied by Arduino-ESP32.

To regenerate it, install and export ESP-IDF v5.5.5, then run:

```sh
tools/build_classic_bluedroid_host.sh
```

The output is ABI-bound to the ESP-IDF/toolchain used by Arduino-ESP32 3.3.x.
The build script rejects a different ESP-IDF tag instead of silently producing
an incompatible library.
