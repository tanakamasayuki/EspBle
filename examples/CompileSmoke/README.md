# CompileSmoke

> 日本語版: [README.ja.md](README.ja.md)

Minimal build-check sketch. It only includes `EspBle.h` and prints the library version, without initializing the BLE stack. Use it to confirm the library compiles and links for your board before trying the functional examples.

## Hardware

- 1 × ESP32-S3 (or any board with the Arduino-ESP32 NimBLE backend); no peer required

## What it does

- Prints the EspBle library version to Serial once at startup
- Does nothing else; the loop is idle

## Key APIs

- `ESPBLE_VERSION_STR` — library version string defined in `espble_version.h`

## Expected Serial output

```
EspBle 0.1.0
```
