# EspBle

[日本語](README.ja.md)

EspBle is a general-purpose Bluetooth Low Energy library for ESP32 Arduino. **It uses the NimBLE backend bundled with Arduino-ESP32 and does not support Bluetooth Classic.** It is intended to provide central and peripheral roles, generic GATT client and server operations, security, and composable profiles on one shared foundation. External NimBLE-Arduino is not a required dependency.

The public API is not stable yet. The first vertical slices of legacy advertising, scanning, central/peripheral connections, generic GATT server/client, notify/indicate, MTU exchange, Just Works and static-passkey pairing/bonding, and HID Keyboard Device/Host are implemented and verified with an automated two-board ESP32-S3 peer test. The current API and `ble.update()` event delivery are still trial-stage ahead of the first release.

See [README.ja.md](README.ja.md) and the Japanese design documents for the current scope, starting with the development status in [docs/STATUS.ja.md](docs/STATUS.ja.md).

## License

MIT License
