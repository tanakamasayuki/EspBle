# EspBle

> 日本語版: [README.ja.md](README.ja.md)

EspBle is a general-purpose Bluetooth Low Energy library for ESP32 Arduino.
**It calls the NimBLE Host API directly** rather than going through
Arduino-ESP32's `BLEDevice`, `BLEClient` or `BLEServer` wrappers. Central and
peripheral roles, GATT client and server operations, security, HID and BLE MIDI
compose on one `EspBle` foundation.

On the original ESP32 — the first-generation chip, the one with no S3 or C3 suffix —
you also get Bluetooth Classic through `EspBleClassic`: SPP, HID device and host,
A2DP, AVRCP and HFP. **The BLE API is the same across the ESP32 family; what changes
from chip to chip is which radio features exist and how much has been verified** —
the table under [Compatibility](#compatibility) lays the differences out.

> [!IMPORTANT]
> **ESP32-S3 / C3 / C6 / H2** are the standard case: EspBle uses the NimBLE host
> that ships with the Core.
> **The original ESP32** has no Core-provided NimBLE, since its prebuilt libraries
> are Bluedroid, so EspBle bundles a NimBLE host for it. Its BLE 4.2 controller
> rules out some features, and in exchange the chip has Bluetooth Classic.
> **The ESP32-P4** has no BLE radio: a slave chip attached over SDIO does the BLE
> work (ESP-Hosted). That slave needs **its own firmware, separate from this
> library**, and what works depends on which chip it is and which version it runs.

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
- **Rely on hardware-tested behavior:** a peer suite running on two connected boards covers
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

| | ESP32-S3 / C3 / C6 / H2 | Original ESP32 | ESP32-P4 + slave (ESP-Hosted) |
|---|---|---|---|
| BLE radio | On-chip | On-chip | **On the slave chip** (over SDIO; needs its own firmware) |
| NimBLE host | From the Core | **Bundled by EspBle** (`src/nimble_esp32/`) | From the Core (HCI carried over SDIO to the slave) |
| Public BLE API | Same | Same | Same |
| Security / bonding | Supported | Supported | **Unavailable** (upstream ECC defect) |
| 2M / Coded PHY | Supported | **Unavailable** (BLE 4.2 controller) | Depends on the slave chip and its firmware (outside the representative suite) |
| Bluetooth Classic | No BR/EDR radio | **Supported** (SPP / HID / A2DP / AVRCP / HFP; core 3.2.0+, HFP audio 3.3.8+) | No BR/EDR radio |
| Verified scope | Every feature, on a two-S3 peer suite (C3 / C6 / H2 are build-verified in CI) | A two-board peer sweep in both roles; only what passes counts | Representative suite (connect, GATT, notify, MTU, Wi-Fi coexistence), **verified only with a C6 slave on firmware 2.12.11** |

Some limits apply across the whole family. Extended and periodic advertising are unavailable on
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

This is an intentional Arduino mixed-library layout: the bundled NimBLE host is
compiled from source, while the separately generated Classic-only Bluedroid host
ships as `src/esp32/libespble_bluedroid_classic.a`. Their build and update
constraints differ, so EspBle deliberately keeps the two artifacts in different
forms.

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
[Classic feature inventory](docs/CLASSIC_FEATURE_INVENTORY.ja.md). Classic has
shipped since 1.3.0.

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

The P4 has no BLE radio, so a chip that has one is attached over SDIO as an
ESP-Hosted slave and used through the ESP-Hosted NimBLE configuration the Core
supplies. **In this setup the BLE radio work happens on the slave, not on the P4.**

> [!IMPORTANT]
> **The slave runs its own ESP-Hosted co-processor firmware, separate from this
> library.** EspBle neither ships nor updates it; flash it yourself with
> `ESP_HostedOTA` from Arduino-ESP32 or Espressif's own procedure. **Behaviour
> depends on that firmware's version** — slave 2.3.2 and 2.12.11 behaved
> differently on our hardware — so keep the host and slave versions matched.
>
> **What works also depends on which chip the slave is**, because the BLE version,
> the supported PHYs and the connection limit come from its radio and firmware. The
> Core 3.3.11 prebuilt libraries for the P4 carry `esp32c6` as the default slave
> target, but from ESP-Hosted 2.12.2 the Core asks the attached co-processor for its
> target name and picks the update firmware by that name, so a slave other than the
> C6 — a C5, for example — is possible in principle. **EspBle's peer tests cover P4 + C6 only.**

Verified host/slave versions, the slave firmware update procedure and the supported
subset are in the Japanese [ESP-Hosted setup guide](docs/ESP_HOSTED_SETUP.ja.md).

With Core 3.3.11 and a C6 slave, a P4 ECC defect in the bundled IDF blocks LE Secure
Connections, bonding and dependent HID paths, and repeated `begin()` after `end()`
is limited as well. Both are recorded in the Japanese
[known limitations](docs/ESP_HOSTED_LIMITATIONS.ja.md).

For a Tab5 or a custom board whose SDIO wiring differs from the generic P4, select
the matching board variant or override the Core's Hosted pins before initialization
(Japanese [pin setup guide](docs/ESP_HOSTED_SETUP.ja.md#sdio-pinの選択と上書き), and
the bilingual [Hosted/CustomPins example](examples/Hosted/CustomPins/)).

### Core version

Development and the peer tests run on arduino-esp32 3.3.11. The minimum
supported core differs per chip, and the reason is always the same question:
who brings the BLE host.

| Target | Where the host comes from | Minimum core | Why there |
| --- | --- | --- | --- |
| Original ESP32 (BLE / Classic) | **EspBle bundles it** (NimBLE source + Classic archive) | **3.2.0** | Nearly independent of the core version: the Classic host ships the API headers it was built against in `src/esp32/include/`, so declarations and struct layouts cannot drift, and only stable platform APIs (FreeRTOS and the like) come from the core. Below 3.2.0 is unmeasured and stops at an `#error` that says so |
| Original ESP32, HFP audio (SCO) only | Same, but the controller is the core's | **3.3.8** | Cores up to 3.3.7 ship a prebuilt controller built with the PCM audio path (for an external codec chip), which cannot carry the HCI-path SCO EspBle uses. Bundling the host does not help: the controller binary belongs to the core |
| ESP32-S3 / C3 / C6 / H2 | **The core's bundled NimBLE** | **3.3.0** | The 3.2.x-generation prebuilt libraries were built with Bluedroid as the BLE host, so the NimBLE EspBle calls does not exist there. The core switched to NimBLE in 3.3.0. Those versions stop at the `EspBle requires the NimBLE backend` `#error` |
| ESP32-P4 (+C6 ESP-Hosted) | **The core provides it** (NimBLE over Hosted) | **3.3.1** | 3.3.0 does not provide NimBLE for the P4's Hosted configuration |

The original-ESP32 range is measured: 3.2.0 through 3.3.11 all compile and link,
and 3.2.1 / 3.3.0 / 3.3.7 / 3.3.8 / 3.3.10 / 3.3.11 are hardware-verified (Japanese
[core-version test plan](docs/PLAN_CORE_VERSION_MATRIX.ja.md)). The other chips'
ranges are measured by CI.

- **Core Compatibility Matrix** workflow → `docs/COMPATIBILITY.<version>.md` (representative BLE examples across arduino-esp32 releases)
- **Board Build Coverage** workflow → `docs/BOARDS.<version>.md` (the examples present when the workflow runs, across ESP32-S3 / ESP32 / C3 / C6 / H2 / P4 at one Core version)

Both are manual (`workflow_dispatch`) because a full sweep rewrites and rebuilds
every sketch. Cores newer than 3.3.11 are treated as unverified rather than
known-broken: on the original ESP32 they build with a `#warning`.

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
- [ESP32-P4 / ESP-Hosted setup](docs/ESP_HOSTED_SETUP.ja.md) (Japanese)
- [ESP32-P4 / ESP-Hosted known limitations](docs/ESP_HOSTED_LIMITATIONS.ja.md) (Japanese)

## Sibling libraries

- [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) — USB Host library; EspBle shares its keyboard-layout tables and HID usage conventions
- [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) — USB Device library used for combination testing

## License

EspBle's original code is licensed under the [MIT License](LICENSE). This
distribution also contains bundled third-party components under their own
licenses; they are not relicensed under MIT. See
[Third-party notices](THIRD_PARTY_NOTICES.md), including the notices accompanying
the vendored NimBLE source and the precompiled Classic-only Bluedroid archive.
