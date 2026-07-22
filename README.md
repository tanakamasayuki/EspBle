# EspBle

> 日本語版: [README.ja.md](README.ja.md)

EspBle is a general-purpose Bluetooth Low Energy library for ESP32 Arduino. **It uses the NimBLE backend bundled with Arduino-ESP32 and does not support Bluetooth Classic.** It provides central and peripheral roles, generic GATT client and server operations, security, and composable profiles on one shared foundation. External NimBLE-Arduino is not a required dependency.

> [!IMPORTANT]
> **The classic ESP32 is not supported.** EspBle requires the NimBLE backend, which the plain `esp32` build does not ship (it defaults to Bluedroid), so it fails to compile there by design. Supported targets are the NimBLE SoCs: **ESP32-S3 / ESP32-C3 / ESP32-C6 / ESP32-H2**. If you need BLE on the classic ESP32, use [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) instead — see [Compatibility](#compatibility).

The public API is not stable yet: this is the trial stage ahead of the first release, and APIs may still change.

## Features

- Legacy advertising and scanning with value-type scan results, non-connectable beacons, iBeacon (`EspBleIBeacon.h`), Eddystone-URL (`EspBleEddystone.h`), and service-data broadcast/receive
- Central and peripheral connections by scan result or address, with stable library connection IDs; multiple simultaneous connections and auto-reconnect (`setAutoReconnect`, off by default) that recovers unexpected drops
- Generic GATT server/client: per-connection database and known-UUID discovery, characteristic/descriptor read and write, operation timeouts with auto-queueing, notify/indicate, subscriptions (persistent subscriptions, on by default, auto-restore on reconnect)
- MTU exchange, connection snapshots, payload-limit validation
- Security: Just Works and static-passkey pairing (LE Secure Connections), bonding, encrypted/authenticated characteristic permissions
- Address privacy: public / random static / Resolvable Private Address (RPA) selectable via `EspBleConfig::ownAddressType`
- Composite HID Device: 6KRO/NKRO keyboard, mouse, consumer/system control, gamepad, and Vendor Input / Output / Feature profiles in one HID / Device Information / Battery service set
- HID Host: cross-report discovery and events for all supported types; keyboard includes 6KRO/NKRO parsing, 256-bit usage snapshots, 19 layouts, and LED output, while Vendor reports are bidirectional
- BLE MIDI Device and Host: timestamp/running-status/SysEx packet codec with `EspBleMidiDevice` / `EspBleMidiHost` helpers following the EspUsbDevice/EspUsbHost MIDI API
- All user callbacks are delivered from `ble.update()` on the loop task — never from the BLE stack task

Everything above is verified with an automated two-board ESP32-S3 peer test suite plus host-side unit tests; see [tests/TEST_PLAN.ja.md](tests/TEST_PLAN.ja.md).

## Compatibility

EspBle requires the **NimBLE backend bundled with Arduino-ESP32**. Cores built with the Bluedroid default (such as the plain `esp32` board) are rejected at compile time with a clear `#error`. The classic ESP32 is therefore **out of scope** for this library. If you need BLE on the classic ESP32, use [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino): it bundles its own NimBLE host stack and runs on the classic ESP32, with a different API from EspBle.

Development and the peer tests run on arduino-esp32 3.3.10. The supported core-version range and per-board build coverage are measured by CI, not maintained by hand:

- **Core Compatibility Matrix** workflow → `docs/COMPATIBILITY.<version>.md` (representative examples across arduino-esp32 releases on ESP32-S3)
- **Board Build Coverage** workflow → `docs/BOARDS.<version>.md` (every example across ESP32-S3 / ESP32 / C3 / C6 / H2 / P4 at one core version)

Both are manual (`workflow_dispatch`) because a full sweep rewrites and rebuilds every sketch. Consult the generated matrix for the authoritative minimum core version.

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

**New here? Start with the [documentation guide](docs/README.md)** — it shows the reading order and each document's role. The quickest path to "where does this project stand" is [docs/STATUS.ja.md](docs/STATUS.ja.md) then [docs/DECISIONS.ja.md](docs/DECISIONS.ja.md).

The design documents are currently written in Japanese.

- [Development status and TODO](docs/STATUS.ja.md)
- [Requirements](docs/REQUIREMENTS.ja.md)
- [Core design](docs/CORE_DESIGN.ja.md)
- [API design](docs/API_DESIGN.ja.md)
- [HID Device specification](docs/HID_DEVICE_SPEC.ja.md)
- [HID Host specification](docs/HID_HOST_SPEC.ja.md)
- [Terminology and naming rules](docs/TERMINOLOGY.ja.md)
- [Design decision ledger](docs/DECISIONS.ja.md)
- [Feature support matrix](docs/FEATURE_MATRIX.ja.md)
- [Development status](docs/STATUS.ja.md)
- [Test plan](tests/TEST_PLAN.ja.md)
- [Release checklist](docs/RELEASE_CHECKLIST.md)

## Sibling libraries

- [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) — USB Host library; EspBle shares its keyboard-layout tables and HID usage conventions
- [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) — USB Device library used for combination testing

## License

MIT License
