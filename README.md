# EspBle

> 日本語版: [README.ja.md](README.ja.md)

EspBle is a general-purpose Bluetooth Low Energy library for ESP32 Arduino. **It uses the NimBLE backend bundled with Arduino-ESP32 and does not support Bluetooth Classic.** It provides central and peripheral roles, generic GATT client and server operations, security, and composable profiles on one shared foundation. External NimBLE-Arduino is not a required dependency.

The public API is not stable yet: this is the trial stage ahead of the first release, and APIs may still change.

## Features

- Legacy advertising and scanning with value-type scan results
- Central and peripheral connections with stable library connection IDs
- Generic GATT server/client: known-UUID discovery, read, write, notify/indicate, subscriptions
- MTU exchange, connection snapshots, payload-limit validation
- Security: Just Works and static-passkey pairing (LE Secure Connections), bonding, encrypted/authenticated characteristic permissions
- HID Keyboard Device: fixed 6KRO report-protocol keyboard with HID / Device Information / Battery services
- HID Keyboard Host: report-map parsing, input subscription, 256-bit usage snapshots, key events with 19 EspUsbHost-compatible keyboard layouts (Unicode conversion), LED output
- All user callbacks are delivered from `ble.update()` on the loop task — never from the BLE stack task

Everything above is verified with an automated two-board ESP32-S3 peer test suite plus host-side unit tests; see [tests/TEST_PLAN.ja.md](tests/TEST_PLAN.ja.md).

## Getting started

Each example ships a `sketch.yaml` pinned to the verified Arduino-ESP32 version:

```sh
arduino-cli compile --profile esp32s3 examples/Gap/Scan
```

See the [examples index](examples/README.md) for the full list with pairing suggestions. A minimal scanner looks like:

```cpp
#include <EspBle.h>

EspBle ble;

void setup() {
  Serial.begin(115200);
  ble.begin();
  ble.scanner().onResult([](const EspBleScanResult &result) {
    Serial.printf("%s RSSI=%d\n", result.address.c_str(), result.rssi);
  });
  ble.scanner().start();
}

void loop() {
  ble.update();  // all callbacks are delivered from here
  delay(1);
}
```

## Documents

The design documents are currently written in Japanese.

- [Development status and TODO](docs/STATUS.ja.md)
- [Requirements](docs/REQUIREMENTS.ja.md)
- [Core design](docs/CORE_DESIGN.ja.md)
- [API design](docs/API_DESIGN.ja.md)
- [HID Keyboard Device specification](docs/HID_KEYBOARD_DEVICE_SPEC.ja.md)
- [HID Keyboard Host specification](docs/HID_KEYBOARD_HOST_SPEC.ja.md)
- [Terminology and naming rules](docs/TERMINOLOGY.ja.md)
- [Design decision ledger](docs/DECISIONS.ja.md)
- [Development plan](docs/DEVELOPMENT_PLAN.ja.md)
- [Test plan](tests/TEST_PLAN.ja.md)

## Sibling libraries

- [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) — USB Host library; EspBle shares its keyboard-layout tables and HID usage conventions
- [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) — USB Device library used for combination testing

## License

MIT License
