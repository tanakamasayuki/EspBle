# EspBle

> 日本語版: [README.ja.md](README.ja.md)

EspBle is a general-purpose Bluetooth Low Energy library for ESP32 Arduino.
**It calls the NimBLE Host API integrated into Arduino-ESP32 as an ESP-IDF
component directly.** It does not go through Arduino-ESP32's `BLEDevice`,
`BLEClient`, or `BLEServer` wrappers. Central and peripheral roles, GATT client
and server operations, security, HID, and BLE MIDI share one `EspBle`
foundation. On the original ESP32 it also offers Bluetooth Classic — SPP, HID
device and host, A2DP, AVRCP and HFP — and running it alongside bundled NimBLE
(dual-host) remains experimental. Which Classic features are
hardware-verified, unverified or unimplemented is tracked per feature; see
[BLE or Bluetooth Classic](docs/CLASSIC_VS_BLE.md).

> [!IMPORTANT]
> EspBle uses the NimBLE backend built into Arduino-ESP32. Native-controller
> targets are **ESP32-S3 / ESP32-C3 / ESP32-C6 / ESP32-H2**.
> **ESP32-P4 + ESP32-C6 is supported with limitations through ESP-Hosted**;
> security/bonding and repeated full reinitialization have upstream limitations.
> **The classic ESP32 works because EspBle bundles a NimBLE host for it, but its
> BLE 4.2 controller rules out some features and only what is verified on
> hardware is supported**; see [Compatibility](#compatibility).

## Why use EspBle?

- **Use the Core-integrated NimBLE stack as-is:** EspBle directly uses the
  ESP-IDF NimBLE Host and controller or ESP-Hosted HCI configuration selected
  and built by Arduino-ESP32. It does not layer another BLE stack into the
  library, and follows the Core's SoC support, configuration, and updates.
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
test suite plus host-side unit tests. On P4/C6 Hosted, connections, GATT,
notify/indicate, MTU, Wi-Fi/BLE coexistence, and shared-transport lifecycle have
been verified with a P4/S3 pair. For excluded security paths and other details,
see [tests/TEST_PLAN.md](tests/TEST_PLAN.md) and the
[ESP-Hosted limitations](docs/ESP_HOSTED_LIMITATIONS.ja.md).

## Compatibility

EspBle directly uses the **NimBLE Host API integrated into Arduino-ESP32 as an
ESP-IDF component**. A configuration that provides no NimBLE is rejected at
compile time with a clear `#error`.

The plain `esp32` board is the one exception: its Arduino-ESP32 prebuilt
libraries are built with Bluedroid, so EspBle bundles a NimBLE host for it
(`src/nimble_esp32/`, the same esp-nimble snapshot the matching esp-idf pins).
Its configuration is frozen to the values the other targets use, and overriding
any of it is rejected.

Support for the classic ESP32 is not on par with the other chips: EspBle carries
the maintenance of the bundled hosts itself. Classic SPP (byte stream and Arduino
`Stream`), generic HID Device/Host, A2DP raw transport, AVRCP CT/TG and HFP
Client/Audio Gateway use a separately built Bluedroid host with those profiles
enabled. Radio settings — transmit power, page timeout and the minimum
encryption key size — are available too, and the composition limit of the HID
Report Descriptor (214 bytes of descriptor plus device strings, shared with one
SDP record) is checked before registering rather than failing silently.
A Classic-only sketch selects this host automatically when it uses
`EspBleClassic`, with no `build_opt.h` required.
Classic HID has the same API shape as BLE HID: the device side offers
`hidKeyboard()`, `hidMouse()`, `hidConsumerControl()`, `hidSystemControl()` and
`hidGamepad()` under the same names and signatures, and the host side parses the
Report Descriptor it receives to deliver keyboard state, per-usage keyboard
events and mouse events. Report Descriptors and report packing come from one
module shared by both transports.
Which hosts run follows only what the sketch calls `begin()` on: one host makes
the broker a pass-through, and starting both `EspBle` and `EspBleClassic` makes
it route HCI between them. There is no build flag. Dual-host is experimental, so
`end()` one of them and keep a single host if it misbehaves. Classic HID
traffic together with an LE connection, repeated GATT reads, and bidirectional HID
traffic afterwards has passed hardware tests. In Classic-only mode, encoded A2DP
Sink/Source media transport, AVRCP playback/absolute-volume control, and HFP
Client/Audio Gateway single-call control plus raw mSBC SCO transport also pass;
dual-host mode also passes bidirectional mSBC SCO while an LE GATT connection
remains usable during and after the audio link. It also passes A2DP encoded-media
streaming and AVRCP playback/volume control while GATT reads remain usable before,
during, and after the stream. In Classic-only mode the Audio Gateway can select
CVSD or mSBC, and both codecs pass raw SCO transfer on hardware; external-device
interoperability remains. The
two HFP roles are process-wide mutually exclusive.
Shared-command scheduling now
uses a broker-owned FIFO and controller command credits. The broker stops the
controller after the final host leaves, independent of host destruction order;
event masks are merged from per-host requests, and Classic can reattach without
resetting or reconfiguring an active LE controller. Pairing, bond persistence,
bond reconnection, and encrypted GATT access also pass while Classic HID remains
connected. Observed-command classification and long-duration load have completed;
wrong-passkey and HID-connection failures recover without dropping the other host,
and backend callback teardown has a reference-lifetime barrier. Classic is part of
the next release, with each feature's state — hardware-verified, unverified or
unimplemented — written down in the
[Classic feature inventory](docs/CLASSIC_FEATURE_INVENTORY.ja.md) (Japanese).
Incoming ACL flow control is broker-owned; outgoing buffers are not apportioned
between the two hosts.
See the [Classic implementation plan](docs/PLAN_ESP32_CLASSIC.ja.md). The classic ESP32
also has a BLE 4.2 controller, so **LE 2M and LE Coded PHY are unavailable**,
extended and periodic advertising are unavailable, and the connection limit is 3.
Only what the on-hardware peer tests cover is considered supported (GATT
read/write/discovery, MTU, connection-parameter updates, pairing and bonding, HID
device, HID host, BLE MIDI device, in both central and peripheral roles); timing
behaviour is not guaranteed to match the other targets. The reasoning and the
verification record are in the Japanese
[original-ESP32 plan](docs/PLAN_ESP32.ja.md).

ESP32-P4 can use the ESP-Hosted NimBLE configuration supplied by Arduino-ESP32.
The verified host/slave versions, C6 update procedure, and supported subset are
documented in the Japanese [ESP-Hosted setup guide](docs/ESP_HOSTED_SETUP.ja.md).
With Core 3.3.11, a P4 ECC defect in the bundled IDF blocks LE Secure
Connections, bonding, and dependent HID paths; repeated `begin()` after `end()`
is also limited. These are covered by the
[known limitations](docs/ESP_HOSTED_LIMITATIONS.ja.md).
For a Tab5 or custom board whose SDIO wiring differs from generic P4, select the
matching board variant or override the Core's Hosted pins before initialization.
See the Japanese [pin setup guide](docs/ESP_HOSTED_SETUP.ja.md#sdio-pinの選択と上書き)
and the bilingual [Hosted/CustomPins example](examples/Hosted/CustomPins/).

Development and the peer tests run on arduino-esp32 3.3.11. The supported core-version range and per-board build coverage are measured by CI, not maintained by hand:

- **Core Compatibility Matrix** workflow → `docs/COMPATIBILITY.<version>.md` (representative examples across arduino-esp32 releases on S3 / C3 / C6 / H2 / P4)
- **Board Build Coverage** workflow → `docs/BOARDS.<version>.md` (every example across ESP32-S3 / ESP32 / C3 / C6 / H2 / P4 at one core version)

Both are manual (`workflow_dispatch`) because a full sweep rewrites and rebuilds every sketch. Consult the generated matrix for the authoritative minimum core version.

## Getting started

Install EspBle from Arduino Library Manager, or with Arduino CLI:

```sh
arduino-cli lib install EspBle
```

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

**New to BLE? Start with the [beginner's guide to BLE](docs/GUIDE_BLE_BASICS.md)** — it explains what is actually happening, from finding a peer through to exchanging data, and links to the matching example for each topic.

**Looking for a specific document? See the [documentation guide](docs/README.md)** — it shows the reading order and each document's role. The quickest path to "where does this project stand" is [docs/STATUS.md](docs/STATUS.md) then [docs/DECISIONS.ja.md](docs/DECISIONS.ja.md).

The user-facing documents below are available in English; the remaining design documents are currently Japanese-only.

- [A beginner's guide to BLE](docs/GUIDE_BLE_BASICS.md)
- [A beginner's guide to Bluetooth Classic](docs/GUIDE_CLASSIC_BASICS.md)
- [BLE or Bluetooth Classic](docs/CLASSIC_VS_BLE.md)
- [Development status and TODO](docs/STATUS.md)
- [Requirements](docs/REQUIREMENTS.ja.md)
- [Core design](docs/CORE_DESIGN.ja.md)
- [API design](docs/API_DESIGN.ja.md)
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
