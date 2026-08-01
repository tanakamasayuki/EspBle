# EspBle 1.1.0 — arduino-esp32 core compatibility

- Targets: esp32s3, esp32c3, esp32c6, esp32h2, esp32p4
- Core versions: 3.2.0, 3.2.1, 3.3.0, 3.3.1, 3.3.2, 3.3.3, 3.3.4, 3.3.5, 3.3.6, 3.3.7, 3.3.8, 3.3.9, 3.3.10, 3.3.11

Legend: ✅ builds · ❌ fails · — example absent in this version · · not applicable (no profile / board not in core)

## esp32s3

| Feature (example) | 3.2.0 | 3.2.1 | 3.3.0 | 3.3.1 | 3.3.2 | 3.3.3 | 3.3.4 | 3.3.5 | 3.3.6 | 3.3.7 | 3.3.8 | 3.3.9 | 3.3.10 | 3.3.11 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Smoke (`CompileSmoke`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GAP (`Gap/Connect`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GATT Server (`Gatt/NotifyServer`) | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| GATT Client (`Gatt/Client`) | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| Security (`Security/StaticPasskeyServer`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HID Device (`Hid/KeyboardDevice`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HID Host (`Hid/KeyboardHost`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

## esp32c3

| Feature (example) | 3.2.0 | 3.2.1 | 3.3.0 | 3.3.1 | 3.3.2 | 3.3.3 | 3.3.4 | 3.3.5 | 3.3.6 | 3.3.7 | 3.3.8 | 3.3.9 | 3.3.10 | 3.3.11 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Smoke (`CompileSmoke`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GAP (`Gap/Connect`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GATT Server (`Gatt/NotifyServer`) | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| GATT Client (`Gatt/Client`) | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| Security (`Security/StaticPasskeyServer`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HID Device (`Hid/KeyboardDevice`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HID Host (`Hid/KeyboardHost`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

## esp32c6

| Feature (example) | 3.2.0 | 3.2.1 | 3.3.0 | 3.3.1 | 3.3.2 | 3.3.3 | 3.3.4 | 3.3.5 | 3.3.6 | 3.3.7 | 3.3.8 | 3.3.9 | 3.3.10 | 3.3.11 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Smoke (`CompileSmoke`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GAP (`Gap/Connect`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GATT Server (`Gatt/NotifyServer`) | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| GATT Client (`Gatt/Client`) | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| Security (`Security/StaticPasskeyServer`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HID Device (`Hid/KeyboardDevice`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HID Host (`Hid/KeyboardHost`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

## esp32h2

| Feature (example) | 3.2.0 | 3.2.1 | 3.3.0 | 3.3.1 | 3.3.2 | 3.3.3 | 3.3.4 | 3.3.5 | 3.3.6 | 3.3.7 | 3.3.8 | 3.3.9 | 3.3.10 | 3.3.11 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Smoke (`CompileSmoke`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GAP (`Gap/Connect`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GATT Server (`Gatt/NotifyServer`) | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| GATT Client (`Gatt/Client`) | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| Security (`Security/StaticPasskeyServer`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HID Device (`Hid/KeyboardDevice`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HID Host (`Hid/KeyboardHost`) | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

## esp32p4

| Feature (example) | 3.2.0 | 3.2.1 | 3.3.0 | 3.3.1 | 3.3.2 | 3.3.3 | 3.3.4 | 3.3.5 | 3.3.6 | 3.3.7 | 3.3.8 | 3.3.9 | 3.3.10 | 3.3.11 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Smoke (`CompileSmoke`) | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GAP (`Gap/Connect`) | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GATT Server (`Gatt/NotifyServer`) | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| GATT Client (`Gatt/Client`) | — | — | — | — | — | — | — | — | — | — | — | — | — | — |
| Security (`Security/StaticPasskeyServer`) | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HID Device (`Hid/KeyboardDevice`) | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HID Host (`Hid/KeyboardHost`) | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

## Failure details

- `Gap/Connect` @ esp32c3 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Connect` @ esp32c3 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Connect` @ esp32c6 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Connect` @ esp32c6 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Connect` @ esp32h2 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Connect` @ esp32h2 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Connect` @ esp32p4 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Connect` @ esp32p4 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Connect` @ esp32p4 / 3.3.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Connect` @ esp32s3 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Connect` @ esp32s3 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardDevice` @ esp32c3 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardDevice` @ esp32c3 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardDevice` @ esp32c6 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardDevice` @ esp32c6 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardDevice` @ esp32h2 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardDevice` @ esp32h2 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardDevice` @ esp32p4 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardDevice` @ esp32p4 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardDevice` @ esp32p4 / 3.3.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardDevice` @ esp32s3 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardDevice` @ esp32s3 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardHost` @ esp32c3 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardHost` @ esp32c3 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardHost` @ esp32c6 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardHost` @ esp32c6 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardHost` @ esp32h2 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardHost` @ esp32h2 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardHost` @ esp32p4 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardHost` @ esp32p4 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardHost` @ esp32p4 / 3.3.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardHost` @ esp32s3 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardHost` @ esp32s3 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyServer` @ esp32c3 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyServer` @ esp32c3 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyServer` @ esp32c6 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyServer` @ esp32c6 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyServer` @ esp32h2 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyServer` @ esp32h2 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyServer` @ esp32p4 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyServer` @ esp32p4 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyServer` @ esp32p4 / 3.3.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyServer` @ esp32s3 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyServer` @ esp32s3 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `CompileSmoke` @ esp32c3 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `CompileSmoke` @ esp32c3 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `CompileSmoke` @ esp32c6 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `CompileSmoke` @ esp32c6 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `CompileSmoke` @ esp32h2 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `CompileSmoke` @ esp32h2 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `CompileSmoke` @ esp32p4 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `CompileSmoke` @ esp32p4 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `CompileSmoke` @ esp32p4 / 3.3.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `CompileSmoke` @ esp32s3 / 3.2.0: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `CompileSmoke` @ esp32s3 / 3.2.1: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`

