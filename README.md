# EspBle

> 日本語版: [README.ja.md](README.ja.md)

EspBle is a general-purpose Bluetooth Low Energy library for ESP32 Arduino.
**It calls the NimBLE Host API directly** rather than going through
Arduino-ESP32's `BLEDevice`, `BLEClient` or `BLEServer` wrappers. Central and
peripheral roles, GATT client and server operations, security, HID and BLE MIDI
compose on one `EspBle` foundation.

On the original ESP32 you also get Bluetooth Classic — SPP, HID device and host,
A2DP, AVRCP and HFP — through `EspBleClassic`. **The BLE API is the same on every
ESP32; what changes per SoC is which radio features exist and how much has been
verified** — the table under [Compatibility](#compatibility) lays the differences out.

> [!IMPORTANT]
> **ESP32-S3 / C3 / C6 / H2** are the standard case: EspBle uses the NimBLE host
> that ships with the Core.
> **The original ESP32** has no Core-provided NimBLE, since its prebuilt libraries
> are Bluedroid, so EspBle bundles a NimBLE host for it. Its BLE 4.2 controller
> rules out some features, and in exchange the chip has Bluetooth Classic.
> **ESP32-P4 + ESP32-C6** works through ESP-Hosted, with upstream limitations
> including security and bonding.

## Why use EspBle?

- **Use the NimBLE stack the Core selected:** EspBle uses the ESP-IDF NimBLE
  host and the controller or ESP-Hosted HCI configuration Arduino-ESP32 selected
  and built, without layering another BLE stack on top. The original ESP32, which
  gets no NimBLE from the Core, is the one place EspBle brings that same host itself.
- **Expose low-level correctness through an Arduino-oriented API:** GATT
  attributes can be addressed by handle as well as UUID, so duplicate service
  and characteristic UUIDs, descriptors, and per-connection discovery snapshots
  remain distinguishable. Asynchronous GATT operations are serialized through
  a timeout-aware queue.
- **Compose central and peripheral roles on one foundation:** scanning,
  advertising, multiple connections, GATT client/server, and pairing/bonding
  are configured through the same `EspBle` instance.
- **Avoid rebuilding HID and BLE MIDI from raw attributes:** keyboard, mouse,
  consumer/system control, gamepad, and custom HID reports can share one HID
  service. HID Host and BLE MIDI Device/Host helpers use the same event model.
- **Know where callbacks run:** asynchronous connection, GATT-completion,
  notification, and HID events are delivered by `ble.update()` on the loop
  task. The synchronous GATT Server `onRead()` hook is the explicit exception
  and runs on the stack task because it must produce the response immediately.
- **Rely on hardware-tested behavior:** a two-board ESP32 peer suite covers
  connections, GATT, security, HID, reconnection, and error paths, supplemented
  by host unit tests and cross-SoC example builds.

## Features

| Area | What it provides | Key behavior and APIs |
| --- | --- | --- |
| Advertising / Scan | Regular advertising, active/passive scanning, non-connectable beacons, iBeacon, and Service Data | Value-type scan results retain address, name, RSSI, service UUIDs, Manufacturer Data, and more. Advertising and Scan Response payloads are configured separately |
| Connections | Central connections by scan result/address, peripheral connection acceptance, disconnect, and multiple simultaneous links | Stable application-facing connection IDs, connection snapshots, parameter/PHY updates, and opt-in auto-reconnect after unexpected drops |
| GATT Server | Custom services, characteristics, and descriptors with read/write and notify/indicate | Targets can be addressed by UUID or attribute handle. Duplicate service/characteristic UUIDs and per-connection subscription state are supported |
| GATT Client | Full-database or known-UUID discovery, characteristic/descriptor read/write, subscribe/unsubscribe | Per-connection discovery snapshots, handle-based access, long reads, timeouts, and an automatic operation queue. Persistent subscriptions restore on reconnect by default |
| ATT / Link | MTU exchange, payload-limit validation, connection parameters, and LE PHY | MTU and link state are reflected in connection snapshots and reported through asynchronous completion events |
| Security / Privacy | LE Secure Connections with Just Works, passkey, and Numeric Comparison; bonding and encrypted/authenticated permissions | Public, random static, and Resolvable Private Address (RPA) modes. Security has documented limitations on P4/C6 Hosted |
| HID Device | Keyboard (6KRO/NKRO), mouse, consumer/system control, gamepad, vendor, and arbitrary custom reports | Compose several profiles into one HID service; includes report sending, battery, LED output, and Boot Keyboard Protocol |
| HID Host | Discovery, subscription, and report parsing for BLE keyboards, mice, gamepads, and related devices | Normalizes 6KRO/NKRO into usage snapshots; supports 19 keyboard layouts, LED output, and vendor Input/Output/Feature reports |
| BLE MIDI | MIDI Device and Host with notes, Control Change, Program Change, Pitch Bend, and SysEx | Helpers/codecs handle timestamps, running status, and SysEx spanning multiple BLE packets |
| Events / Lifecycle | Asynchronous connection, GATT-completion, notification, security, and HID events plus `begin()` / `end()` | Asynchronous callbacks run from the loop task through `ble.update()`. Only the synchronous GATT Server `onRead()` hook runs on the stack task |

See the [feature matrix](docs/FEATURE_MATRIX.md) for API-level support and
limitations, and the [examples index](examples/README.md) for task-oriented
examples.

The full feature set above is verified with an automated two-board ESP32-S3 peer
suite plus host-side unit tests. What is covered per SoC is in
[Compatibility](#compatibility) below; the suite-by-suite detail is in
[tests/TEST_PLAN.md](tests/TEST_PLAN.md).

## Compatibility

### What differs per SoC

| | ESP32-S3 / C3 / C6 / H2 | Original ESP32 | ESP32-P4 + ESP32-C6 |
|---|---|---|---|
| NimBLE host | From the Core | **Bundled by EspBle** (`src/nimble_esp32/`) | From the Core (HCI over SDIO to the C6) |
| Public BLE API | Same | Same | Same |
| Security / bonding | Supported | Supported | **Unavailable** (upstream ECC defect) |
| 2M / Coded PHY | Supported | **Unavailable** (BLE 4.2 controller) | Outside the representative suite |
| Bluetooth Classic | No BR/EDR radio | **Supported** (SPP / HID / A2DP / AVRCP / HFP) | No BR/EDR radio |
| Verified scope | Every feature, on a two-S3 peer suite (C3 / C6 / H2 are build-verified in CI) | A two-board peer sweep in both roles; only what passes counts | Representative suite (connect, GATT, notify, MTU, Wi-Fi coexistence) |

Some limits apply everywhere. Extended and periodic advertising are unavailable on
every target, because the NimBLE the Core ships is built with
`CONFIG_BT_NIMBLE_EXT_ADV` disabled. The simultaneous-connection limit comes from
the controller and is 3 in the verified configurations. For API-level support, see
the [feature matrix](docs/FEATURE_MATRIX.md).

A configuration that provides no NimBLE is rejected at compile time with a clear
`#error`.

### Why the original ESP32 is different

**1. EspBle brings the BLE host.** This is the one chip whose Arduino-ESP32
prebuilt libraries are Bluedroid, so no Core-provided NimBLE exists. EspBle bundles
the same esp-nimble snapshot the matching esp-idf pins into `src/nimble_esp32/`,
frozen to the configuration the other targets use (overriding it is rejected).
**Carrying that host is maintenance the library takes on**, so support here is not
on par with the other chips: only what the on-hardware peer tests cover counts as
supported — GATT read/write/discovery, MTU, connection-parameter updates, pairing
and bonding, HID device, HID host, BLE MIDI device, in both central and peripheral
roles — and timing behaviour is not guaranteed to match the other targets. The
reasoning and the record are in the Japanese
[original-ESP32 plan](docs/PLAN_ESP32.ja.md).

**2. It has a BLE 4.2 controller.** LE 2M and LE Coded PHY are unavailable.

**3. It has Bluetooth Classic.** It is the only Arduino-ESP32 target with a BR/EDR
radio. Classic runs on a separately built, namespaced Bluedroid host with the
needed profiles enabled.

- SPP (as a byte stream and as an Arduino `Stream`), generic HID Device/Host, A2DP
  raw transport, AVRCP CT/TG, and HFP Client/Audio Gateway
- Radio settings: transmit power, page timeout, and the minimum encryption key size
- HID keeps the same API shape as BLE: `hidKeyboard()`, `hidMouse()`,
  `hidConsumerControl()`, `hidSystemControl()` and `hidGamepad()` under the same
  names and signatures, with Report Descriptors and report packing coming from one
  module shared by both transports
- The composition limit of the HID Report Descriptor — 214 bytes of descriptor plus
  device strings, sharing one SDP record — is checked before registration instead of
  failing silently

Which radio suits which peer is covered in
[BLE or Bluetooth Classic](docs/CLASSIC_VS_BLE.md), and each feature's state —
hardware-verified, unverified or unimplemented — is written down in the Japanese
[Classic feature inventory](docs/CLASSIC_FEATURE_INVENTORY.ja.md). Classic is part
of the next release.

**Running BLE and Classic together (dual-host) is experimental.** Which hosts run
follows only what the sketch calls `begin()` on; there is no build flag. With one
host the HCI broker in between is a pass-through, and starting both `EspBle` and
`EspBleClassic` makes it route HCI between them. An LE connection alongside Classic
HID traffic, repeated GATT reads, and A2DP / AVRCP / HFP mSBC SCO while an LE GATT
connection stays live all pass on hardware, but constraints remain — outgoing
buffers are not apportioned between the two hosts, for one — so it stays
experimental. If it misbehaves, `end()` one of them and keep a single host. See the
Japanese [Classic implementation plan](docs/PLAN_ESP32_CLASSIC.ja.md) and
[STATUS](docs/STATUS.md).

### ESP32-P4 + ESP32-C6 (ESP-Hosted)

The P4 has no BLE radio, so a C6 is attached over SDIO as an ESP-Hosted slave and
used through the ESP-Hosted NimBLE configuration the Core supplies. Verified
host/slave versions, the C6 update procedure and the supported subset are in the
Japanese [ESP-Hosted setup guide](docs/ESP_HOSTED_SETUP.ja.md).

With Core 3.3.11, a P4 ECC defect in the bundled IDF blocks LE Secure Connections,
bonding and dependent HID paths, and repeated `begin()` after `end()` is limited
as well. Both are recorded in the Japanese
[known limitations](docs/ESP_HOSTED_LIMITATIONS.ja.md).

For a Tab5 or a custom board whose SDIO wiring differs from the generic P4, select
the matching board variant or override the Core's Hosted pins before initialization
(Japanese [pin setup guide](docs/ESP_HOSTED_SETUP.ja.md#sdio-pinの選択と上書き), and
the bilingual [Hosted/CustomPins example](examples/Hosted/CustomPins/)).

### Core version

Development and the peer tests run on arduino-esp32 3.3.11. The supported
core-version range and per-board build coverage are measured by CI, not maintained
by hand:

- **Core Compatibility Matrix** workflow → `docs/COMPATIBILITY.<version>.md` (representative examples across arduino-esp32 releases on S3 / C3 / C6 / H2 / P4)
- **Board Build Coverage** workflow → `docs/BOARDS.<version>.md` (every example across ESP32-S3 / ESP32 / C3 / C6 / H2 / P4 at one core version)

Both are manual (`workflow_dispatch`) because a full sweep rewrites and rebuilds
every sketch. Consult the generated matrix for the authoritative minimum core
version.

## Getting started

Install EspBle from Arduino Library Manager, or with Arduino CLI:

```sh
arduino-cli lib install EspBle
```

Each example ships a `sketch.yaml` pinned to the verified Arduino-ESP32 version:

```sh
arduino-cli compile --profile esp32s3 examples/Gap/Scan
```

The Bluetooth Classic examples are original-ESP32 only, so they build with the `esp32` profile:

```sh
arduino-cli compile --profile esp32 examples/Classic/SppServer
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

**New to BLE? Start with the [beginner's guide to BLE](docs/GUIDE_BLE_BASICS.md)** — it explains what is actually happening, from finding a peer through to exchanging data, and links to the matching example for each topic.

**Once you have a sketch that works, read [EspBle in depth](docs/GUIDE_ADVANCED.md)** — which task your callbacks run on, every capacity and what overflowing it does, backpressure, reconnection, dual-host internals, measuring footprint, and a playbook for known failure signatures.

**Looking for a specific document? See the [documentation guide](docs/README.md)** — it shows the reading order and each document's role. The quickest path to "where does this project stand" is [docs/STATUS.md](docs/STATUS.md) then [docs/DECISIONS.ja.md](docs/DECISIONS.ja.md).

The user-facing documents below are available in English; the remaining design documents are currently Japanese-only.

- [A beginner's guide to BLE](docs/GUIDE_BLE_BASICS.md)
- [A beginner's guide to Bluetooth Classic](docs/GUIDE_CLASSIC_BASICS.md)
- [BLE or Bluetooth Classic](docs/CLASSIC_VS_BLE.md)
- [EspBle in depth (advanced)](docs/GUIDE_ADVANCED.md)
- [Coming from another library](docs/GUIDE_MIGRATION.md)
- [Writing a HID Report Descriptor](docs/GUIDE_HID_DESCRIPTORS.md)
- [Development status and TODO](docs/STATUS.md)
- [Requirements](docs/REQUIREMENTS.ja.md)
- [Core design](docs/CORE_DESIGN.ja.md)
- [API design](docs/API_DESIGN.md)
- [HID Device specification](docs/HID_DEVICE_SPEC.ja.md)
- [HID Host specification](docs/HID_HOST_SPEC.ja.md)
- [Terminology and naming rules](docs/TERMINOLOGY.ja.md)
- [Design decision ledger](docs/DECISIONS.ja.md)
- [Feature support matrix](docs/FEATURE_MATRIX.md)
- [Test plan](tests/TEST_PLAN.md)
- [Release checklist](docs/RELEASE_CHECKLIST.md)

## Sibling libraries

- [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) — USB Host library; EspBle shares its keyboard-layout tables and HID usage conventions
- [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) — USB Device library used for combination testing

## License

MIT License
