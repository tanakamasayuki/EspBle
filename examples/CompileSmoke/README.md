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
EspBle <version>
```

`<version>` is the library version defined in `espble_version.h`.

## Before running this sketch

See the [example workflow](../../docs/GUIDE_EXAMPLES.md) for installation, board selection and the distinction between connection and subscription. Open [CompileSmoke.ino](CompileSmoke.ino), select the board and port, upload, and match the Serial monitor to `Serial.begin(...)`. This example has no input commands.

## How this example is structured

CompileSmoke - minimal sketch that only checks the library builds and links on the target board. It does not initialize the BLE stack; it just includes the header and prints the library version.

## Verify and adapt

This sketch only includes the library and prints its version. Passing compilation or seeing the version does not test stack initialization, radio operation, pairing or interoperability.

[Example index](../README.md) · [Example workflow](../../docs/GUIDE_EXAMPLES.md)
