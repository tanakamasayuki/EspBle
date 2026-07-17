# CompileSmoke

> 日本語版: [README.ja.md](README.ja.md)

Minimal build-check sketch. It only includes `EspBle.h` and prints the library version, without initializing the BLE stack. Use it to confirm that the library compiles and links for your board before trying the functional examples.

## Hardware

- Any ESP32 board for which Arduino-ESP32 provides the NimBLE backend (initial target: ESP32-S3)

## What it does

- Prints the EspBle library version to Serial once at startup

## Key APIs

- `ESPBLE_VERSION_STR` — library version string defined in `espble_version.h`

## Expected Serial output

```
EspBle 0.1.0
```
