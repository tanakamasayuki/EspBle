# Test Plan

> 日本語版: [TEST_PLAN.ja.md](TEST_PLAN.ja.md)

## Policy

BLE connection, disconnection, discovery, subscription, security, and bonding span multiple asynchronous events. Peer tests are therefore a primary implementation tool, not merely supplementary smoke tests.

- unit: validate keymap conversion, the HID Report Map parser, and similar pure logic with host-side g++ (`tests/unit/`).
- examples_compile: detect public-API and target-SoC build regressions. `.github/workflows/compile-examples.yml` compiles every example with the esp32s3 profile on pushes and pull requests. A ✅ in the build column of the coverage table refers to this check.
- peer: use two ESP32-S3 boards as the baseline fixture and exercise the real radio, controller, and host stack. An ESP32-P4 + ESP32-C6 fixture additionally covers the ESP-Hosted path.
- manual: validate interoperability with Android, iOS, Windows, Linux, macOS, and commercial devices.

There is currently no single-board `single` layer for runtime behavior that does not need a peer. Add it when such scenarios are needed.

## Peer Hardware

Peer tests use two fixture configurations.

| Fixture | Parent DUT | Second peer | Purpose | Connection policy |
|---|---|---|---|---|
| Baseline regression | ESP32-S3 | ESP32-S3 | All EspBle features and the normal NimBLE path | Keeping both connected is recommended |
| ESP-Hosted regression | ESP32-P4 + ESP32-C6 | ESP32-S3 | SDIO, ESP-Hosted, C6 controller, and Wi-Fi/BLE coexistence | Connect only when required |

The baseline reuses the two ESP32-S3 boards kept connected for EspUsbHost/EspUsbDevice. No signal wiring is required between BLE peers; each board only needs serial, power, and a connection to the test host.

One additional ESP32-S3 is available for manual tests. A future three-board scenario can add another peer directory and profile/port configuration and use it as the third peer. The always-connected two-board fixture remains the minimum automated setup; use three boards for multiple connections or BLE-to-BLE bridge E2E tests.

The setup follows the existing pytest-embedded-cli conventions:

- normal parent profile: `s3_peer_host`
- P4 parent profile: `p4_peer_host`
- second-board profile: `s3_peer_device`
- second-board directory: `peer_device/`
- Python fixture: `peers["device"]`

`host` and `device` describe neither USB roles nor BLE roles. pytest-embedded-cli flashes and runs both sketches and lets the test observe and control both `dut` and `peers["device"]` serial streams.

Initial scenarios keep the parent sketch as Central and the second-board sketch as Peripheral. Assertions primarily inspect the parent when testing an EspBle Central and the peer when testing an EspBle Peripheral. The tests do not depend on swapping roles or source placement.

## P4/C6 ESP-Hosted Regression

### Why P4 hardware testing is required

A P4 compile does not exercise the SDIO transport between P4 and C6, ESP-Hosted initialization and teardown, the C6 controller, or shared Wi-Fi/BLE ownership. S3-only peer tests cannot reproduce this path. Maintaining P4 support therefore requires an additional P4+C6 hardware regression. One fixture is sufficient and it does not need to remain connected.

### Reference fixture requirements

- Use ESP32-P4 as the host and ESP32-C6 as the ESP-Hosted slave/controller. The second ESP32-S3 is a wireless peer and has no signal wiring to the P4.
- Connect P4 and C6 using 4-bit SDIO `CLK`, `CMD`, `D0` through `D3`, `RESET`, and stable power/GND.
- Flash C6 with ESP-Hosted Slave firmware compatible with the Host bundled in Arduino-ESP32 Core. See the [ESP-Hosted setup guide (Japanese)](../docs/ESP_HOSTED_SETUP.ja.md) for preparation and version requirements.
- Prefer an Espressif ESP32-P4-Function-EV-Board with its onboard C6, or an equivalent fixture using the standard wiring on the P4 side: `CLK=18`, `CMD=19`, `D0=14`, `D1=15`, `D2=16`, `D3=17`, and `RESET=54`. This matches Arduino-ESP32's generic `esp32p4` variant and provides a reproducible baseline without a board-specific override.
- Boards with different wiring, such as M5Stack Tab5, are valid when the correct board variant is selected or `hostedSetPins()` is called before `ble.begin()`. See [SDIO pin selection and override (Japanese)](../docs/ESP_HOSTED_SETUP.ja.md#sdio-pinの選択と上書き), and record the board/profile and pin configuration with the result.

A custom-wired fixture is useful as additional coverage, but should not be the only reference because it makes regressions against the Core defaults harder to judge. When adding another P4+C6 fixture, prefer a Function-EV-Board or the standard wiring above. Do not assume the C6 firmware shipped with a board is suitable; verify compatibility before testing and use the official updater when necessary.

### Frequency

| Trigger | P4 hardware test requirement |
|---|---|
| Ordinary change | Use the full S3 regression; P4 does not need to run continuously |
| Changes to `begin()`/`end()`, NimBLE lifecycle, ESP-Hosted branches, P4 profiles, or Wi-Fi coexistence | Run the representative suite for each change or before merge |
| Arduino-ESP32 Core or C6 firmware update | Run the representative suite immediately and re-evaluate known limitations |
| Release candidate | The final candidate must pass the representative suite and Wi-Fi/BLE coexistence test |

During active Hosted development, connect the fixture for each coherent set of changes. When there are no Hosted-related changes, calendar-based weekly runs are not required; connecting it before release is sufficient.

### Default runs and profile selection

An unqualified `pytest` or `pytest peer/` must complete using only the two S3 boards that can remain connected. An optional fixture such as P4/C6 must never be a `default_profile`; explicitly pass `--profile p4_peer_host` only when validating P4 hardware.

`wifi_ble_coexistence` is an ESP-Hosted-specific scenario, but its sketch defaults to `s3_peer_host`. The S3 build excludes Hosted-only headers and APIs and reports `ESP_HOSTED_CAPABLE 0` over serial. pytest checks that capability and completes successfully, so the normal suite does not fail when P4 is disconnected. The P4 build reports `ESP_HOSTED_CAPABLE 1`; only then does the test run Wi-Fi connection, BLE traffic, and shared-transport lifecycle checks.

Successful completion on S3 means that the fixture explicitly reported the scenario as not applicable. It is not evidence that Wi-Fi/BLE coexistence passed. A coexistence regression result is valid only when the P4 profile was explicitly selected.

With the P4 and peer-S3 ports configured in `.env`, run the representative suite from `tests/`:

```sh
uv run --env-file .env pytest \
  peer/stack_smoke/ \
  peer/connect_disconnect/ \
  peer/gatt_read_write/ \
  peer/notify_indicate/ \
  peer/mtu/ \
  peer/wifi_ble_coexistence/ \
  --profile p4_peer_host \
  --peer-profile device:s3_peer_device
```

For a quick connectivity check, run only `peer/connect_disconnect/` with the same profile arguments. Security and repeated full initialization/deinitialization affected by current Core/ESP-Hosted limitations are not mandatory pass criteria for the representative suite. Re-run them after a Core or C6 firmware update and check whether the [known ESP-Hosted limitations (Japanese)](../docs/ESP_HOSTED_LIMITATIONS.ja.md) have been resolved.

## Original-ESP32 Regression

The original ESP32's Arduino-ESP32 prebuilt libraries are built with Bluedroid, so it runs on the
NimBLE host EspBle bundles (`src/nimble_esp32/`). The policy and the verification record live in the
Japanese [original-ESP32 plan](../docs/PLAN_ESP32.ja.md).

| Role | profile | suites |
|---|---|---|
| parent (central) | `esp32_peer_host` | the 62 suites whose parent sketch uses EspBle |
| peer (peripheral) | `esp32_peer_device` | the 64 suites whose peer sketch uses EspBle |

```sh
uv run --env-file .env pytest peer/<suite>/ --profile esp32_peer_host --peer-profile device:s3_peer_device
uv run --env-file .env pytest peer/<suite>/ --profile s3_peer_host --peer-profile device:esp32_peer_device
```

**A side without an esp32 profile skips itself when the profile is selected**, so no exclusions are
needed. Only two kinds of sketch are left without one:

1. **The side written against the core's bundled `BLE` wrapper.** Each suite implements one side with
   EspBle and the other with the bundled wrapper as an independent reference. On the original ESP32
   that wrapper is Bluedroid, which cannot share the controller with our own NimBLE host, so it is
   rejected by `#error`. That is the parent side of `stack_smoke`, `advertise_payload`,
   `hid_keyboard_device` and `midi_device`, and the peer side of `stack_smoke` and `midi_host`.
   **The opposite side of each suite does have a profile and passes on the original ESP32**, so MIDI
   and HID are covered there. Only `stack_smoke`, whose two sides are both wrapper-based, has no
   original-ESP32 coverage -- it is a smoke test of the bundled wrapper, not of EspBle.
2. **`phy_update`** -- the original ESP32 has a BLE 4.2 controller and no LE 2M PHY, so it cannot pass.

When re-running the same suite with the roles swapped, **flash one of the boards with another suite
first**. A suite that selects its target by service UUID, such as `local_identity`, otherwise observes
the board still advertising the previous run's peer firmware.

The two boards are permanently wired on `/dev/ttyUSB0` and `/dev/ttyUSB1`. They are shared with
EspBleBluedroid, and running both repositories' suites at the same time is fine (pytest arbitrates
the ports).

| Trigger | Original-ESP32 run |
|---|---|
| documentation-only change | none |
| ordinary `src/` change | representative smoke (`gatt_read_write`, `security_bond`, `hid_keyboard_host`, `mtu`, `connection_parameters`; parent role is enough, ~15 min) |
| changes to `src/nimble_esp32/`, `EspBleNimbleHost.h`, lifecycle, controller paths or the vendor tool | full sweep in both roles (~1 h each) |
| Arduino-ESP32 core update | full sweep in both roles, and check that the pins in `tools/vendor_nimble_esp32.py` match that core's esp-idf |
| release candidate | full sweep in both roles (see the [release checklist](../docs/RELEASE_CHECKLIST.md)) |

The original ESP32 runs the host **EspBle bundles**, not the core's, so a `src/` change can affect it
in ways the S3 fixture cannot reproduce. The host is the same snapshot as the other targets, though,
so a full sweep every time is not required -- the table above is the granularity.

## Peer-Test Principles

- Use a dedicated 128-bit Service UUID to exclude unrelated nearby BLE devices.
- Do not select a peer by device name alone.
- Where practical, implement one side directly with the BLE API bundled in Arduino-ESP32.
- Keep parent Central and peer Peripheral roles fixed while placing EspBle on the parent, peer, or both according to the test purpose.
- Make every scenario decidable from serial logs.
- Stop scanning, advertising, subscriptions, and connections at the end of each test.
- Explicitly control bond/NVS state at the start and end of security tests.
- Allow timeouts for temporary radio delays, but do not hide defects with unlimited retries.
- Cross-check disconnect reasons, MTU, and security state on both sides where possible.

## Cross-Stack Interoperability Tests (Planned)

Bundled-NimBLE-to-bundled-NimBLE tests cannot reveal every accidental dependency on NimBLE-specific behavior. Add interoperability tests against the sibling **EspBleBluedroid** library, which uses the ESP32 Bluedroid stack.

- Scope: GATT Server/Client in both directions, Notify/Indicate, MTU exchange, and Pairing/Bonding.
- Purpose: verify wire formats and procedures with a NimBLE ↔ Bluedroid combination.
- Timing: after the EspBleBluedroid GATT implementation is operational.

This extends the principle of directly implementing one side with the bundled API, but provides stronger validation because the stack itself differs. It also shows how wrapper-specific constraints, such as duplicate UUID handling, appear to another stack.

## Three-Board Peers (Manual Tests)

Scenarios that require a third board live under `tests/manual/`. They are not part of the default `pytest peer/` run and automatically skip when the third port is unset. Run them from `tests/` with `uv run --env-file .env pytest manual/`; see [tests/manual/README.md](manual/README.md) for boards and ports.

- ✅ `manual/multi_connection`: one Central connects to two Peripherals (`peer_device` and `peer_device2`), verifies notification routing, then handles an unexpected Peripheral-A disconnect. With `setAutoReconnect(true)`, the Central reconnects to the same address and restores the persistent subscription while Peripheral B remains connected. This validates per-connection discovery/subscription isolation and that one peer's disconnect/reconnect does not disturb the other.

Future candidates include two Centrals connected to one Peripheral, and BLE HID input Peripheral → Bridge DUT (Central + Peripheral) → output-verification Central. Add their peer directories and profile/port configuration under `tests/manual/`.

## Coverage Plan

| Area | unit | build | peer | manual |
|---|---|---|---|---|
| Test fixture / bundled BLE stack | | ✅ | `stack_smoke` | |
| Advertising / Scan parser | planned | ✅ | ✅ `advertise_scan` / `advertise_payload` (raw AD structures) | generic scanner |
| Connect / disconnect / timeout | state transitions | ✅ | ✅ `connect_disconnect` / `lifecycle_stress` | |
| Connection lifecycle / event queue / leak | | ✅ | ✅ `lifecycle_stress` | |
| GATT discovery and Characteristic/Descriptor read/write | codec | ✅ | ✅ `gatt_read_write` | generic GATT app |
| Notify / Indicate / unsubscribe | queue | ✅ | ✅ `notify_indicate` / `persistent_subscribe` | generic GATT app |
| MTU | validation | ✅ | ✅ `mtu` | |
| Pairing / Bonding | error/state | ✅ | ✅ `security_bond` | Android/Linux |
| Static passkey / MITM | validation | ✅ | ✅ `security_passkey` | Android/Linux |
| Encrypted characteristic | permission | ✅ | ✅ `security_bond` | |
| Authenticated characteristic | permission | ✅ | ✅ `security_passkey` | |
| HID over GATT security | | ✅ | ✅ `hid_security` | OS |
| Reconnect / peer loss | state | ✅ | ✅ bond reconnect / `lifecycle_stress` / `persistent_subscribe` | `manual/multi_connection` |
| Concurrent connections / isolation | | | | `manual/multi_connection` |
| Address privacy | | ✅ | ✅ `address_privacy` | manual RPA rotation and bonded-peer resolution |
| iBeacon broadcast / decode | ✅ `unit/ibeacon` | ✅ | ✅ `ibeacon` | iBeacon app |
| Advertising Service Data | | ✅ | ✅ `service_data` | generic scanner |
| Scan Response / Appearance / Tx Power | | ✅ | ✅ `scan_response` | generic scanner |
| Filter Accept List / connect timeout | | ✅ | ✅ `accept_list` | |
| Local address / Tx Power / disconnect reason | | ✅ | ✅ `local_identity` | |
| HID Keyboard Device | report codec planned | ✅ | ✅ `hid_keyboard_device` / `hid_robustness` | OS / commercial HID host |
| HID NKRO Device / Host | ✅ `unit/report_map` | ✅ | ✅ `hid_keyboard_nkro` | OS / commercial HID host |
| HID LED output | report codec planned | ✅ | ✅ `hid_keyboard_device` / `hid_keyboard_host` | OS |
| Battery Service | 1-byte codec | ✅ | ✅ HID integration / `battery_service` | generic GATT app |
| Device Information Service | 7-byte PnP ID codec | ✅ | ✅ HID integration / `device_information` | generic GATT app |
| Current Time Service | 10-byte codec | ✅ | ✅ `current_time` | generic GATT app |
| Heart Rate Service | flags / variable-length codec | ✅ | ✅ `heart_rate` | generic GATT app |
| Environmental Sensing Service | signed/scaled integer codec | ✅ | ✅ `environmental_sensing` | generic GATT app |
| BLE MIDI Device / Host | ✅ `unit/midi` | ✅ | ✅ `midi_device` / `midi_host` | commercial instrument / DAW |
| Health Thermometer Service | ✅ `unit/medical_float` | ✅ | ✅ `health_thermometer` | app / commercial thermometer |
| Blood Pressure Service | ✅ `unit/medical_float` | ✅ | ✅ `blood_pressure` | app / commercial monitor |
| Weight Scale Service | fixed-resolution uint16 | ✅ | ✅ `weight_scale` | app / commercial scale |
| Body Composition Service | uint16 flags / optional fields | ✅ | ✅ `body_composition` | app / commercial scale |
| Cycling Speed and Cadence Service | multi-field integer layout | ✅ | ✅ `cycling_speed_cadence` | app / commercial CSC sensor |
| Running Speed and Cadence Service | mixed-width integer layout | ✅ | ✅ `running_speed_cadence` | app / commercial RSC sensor |
| Cycling Power Service | signed 16-bit power / flags | ✅ | ✅ `cycling_power` | app / commercial power meter |
| Fitness Machine Service | flags-driven offset walk | ✅ | ✅ `fitness_machine` | Zwift / commercial trainer |
| Pulse Oximeter Service | SFLOAT | ✅ | ✅ `pulse_oximeter` | app / commercial oximeter |
| Glucose Service / RACP | SFLOAT / date_time | ✅ | ✅ `glucose` | app / commercial meter |
| Location and Navigation Service | flags-driven variable layout / sint32 | ✅ | ✅ `location_navigation` | app / commercial GPS sensor |
| User Data Service | writable characteristic / notify | ✅ | ✅ `user_data` | generic GATT app |
| Alert Notification Service | bitmask / Control Point / notify | ✅ | ✅ `alert_notification` | app / commercial notifier |
| Immediate Alert Service | Write Without Response | ✅ | ✅ `immediate_alert` | app / Find Me tag |
| Phone Alert Status Service | read/notify / WWR Control Point | ✅ | ✅ `phone_alert_status` | app / phone proxy |
| Proximity | two services / signed int8 | ✅ | ✅ `proximity` | app / proximity tag |
| Reference Time Update Service | WWR Control Point / readable state | ✅ | ✅ `reference_time_update` | generic GATT app |
| Bond Management Service | feature bitmask / Control Point | ✅ | ✅ `bond_management` | generic GATT app |
| Continuous Glucose Monitoring Service | ✅ `unit/cgm_crc` | ✅ | ✅ `continuous_glucose_monitoring` | app / commercial CGM |
| HID Keyboard Host | ✅ `unit/report_map` | ✅ | ✅ `hid_keyboard_host` / `hid_boot_keyboard` / `hid_robustness` | commercial keyboard |
| HID keyboard event / layout | ✅ `unit/keymap` | ✅ | ✅ EN-US / JA-JP / en-GB / de-DE / fr-FR | remaining layouts on hardware |
| ESP32KeyBridge input adapter | bridge core | ✅ | · (verified in ESP32KeyBridge) | BLE-to-USB E2E |
| Simultaneous Central + Peripheral | state | planned | planned | |

## Implemented Scenarios

1. ✅ `stack_smoke`: connects two boards using the bundled NimBLE-backend BLE API and verifies read/write plus both serial streams.
2. ✅ `advertise_scan`: EspBle Advertising builder and Scanner parser.
3. ✅ `connect_disconnect`: connection identity, local role, and loop-context connect/disconnect delivery.
4. ✅ `gatt_read_write`: generic GATT Server/Client, list and known-UUID discovery, writes with/without response, descriptor access, and operation timeouts.
5. ✅ `notify_indicate`: subscribe, unsubscribe, CCCD, send results, and received-event queue.
6. ✅ `mtu`: MTU exchange, snapshots/callbacks on both sides, maximum payload, and oversized-payload rejection.
7. ✅ `security_bond`: pairing, bond save/delete/reconnect, and encrypted characteristics.
8. ✅ `security_passkey`: static passkey, MITM, and authenticated characteristics.
9. ✅ `hid_keyboard_device`: HID Keyboard Device, Battery read, Input notification, and Output report.
10. ✅ `hid_keyboard_host`: HID Host, all six Report Map profiles, Vendor Input/Output/Feature, state, and LED return.
11. ✅ `lifecycle_stress`: disconnect under event flood, reconnect heap stability, `end()` during GATT/connect, scanner flush, asynchronous timeout, queued GATT operations, and supervision-timeout peer loss.
12. ✅ `hid_robustness`: CCCD gate, rollover handling, all-release preservation under queue pressure, disconnect rejection during discovery, and incompatible repeated `begin()` rejection.
13. ✅ `hid_security`: a secured HID Device rejects unencrypted read, discovery, and input delivery.
14. ✅ `hid_boot_keyboard`: Report-ID-less boot-keyboard discovery/input and invalid-length counting.
15. ✅ `advertise_payload`: raw advertisement AD structure, single Complete List, and no duplicated AD type.
16. ✅ host unit tests (`tests/unit/`): keymap conversion, HID Report Map parsing, codecs, the HCI router, and the HCI command scheduler.
17. ✅ `battery_service`: standalone Battery Level read, subscription, notification, and unsubscribe.
18. ✅ `device_information`: DIS string reads and 7-byte little-endian PnP ID decoding.
19. ✅ `current_time`: 10-byte Current Time decoding, subscription, notification, and unsubscribe.
20. ✅ `heart_rate`: Body Sensor Location and flags-driven variable Measurement decoding.
21. ✅ `environmental_sensing`: scaled Temperature/Humidity/Pressure reads and Temperature notification.
22. ✅ `hid_keyboard_nkro`: 29-byte bitmap reports, eight simultaneous keys, high usages, individual release, LEDs, and explicit rejection of `preferredMtu` below 32 without silently changing application configuration.
23. ✅ `midi_device`: exact BLE MIDI wire format, empty read, independent multi-packet SysEx reassembly, and Central-to-Device decoding.
24. ✅ `midi_host`: running-status decoding and Host transmission of Note On and multi-packet SysEx.
25. ✅ `health_thermometer`: IEEE-11073 FLOAT Temperature Measurement indication, Type read, and decoding.
26. ✅ `blood_pressure`: SFLOAT systolic/diastolic/mean indication, Feature read, and decoding.
27. ✅ `weight_scale`: 0.005 kg-resolution uint16 Weight Measurement indication and decoding.
28. ✅ `cycling_speed_cadence`: multi-field wheel/crank Measurement notification and decoding.
29. ✅ `running_speed_cadence`: mixed-width speed/cadence/stride/distance notification and decoding.
30. ✅ `cycling_power`: flags plus signed power notification and negative-value decoding.
31. ✅ `pulse_oximeter`: SFLOAT SpO2/pulse-rate Spot-Check indication and decoding.
32. ✅ `glucose`: RACP write → Measurement notify → RACP response indicate, including sequence, base time, and SFLOAT concentration.
33. ✅ `body_composition`: required Body Fat Percentage plus optional Weight indication and full decoding.
34. ✅ `location_navigation`: flags-driven Location and Speed notification with speed and sint32 latitude/longitude.
35. ✅ `user_data`: First Name/Age writes, server `onWritten`, Database Change Increment notify, and persisted-value reread.
36. ✅ `alert_notification`: category bitmask read, Control Point write, and New Alert category/count/text notification.
37. ✅ `immediate_alert`: High/No Alert Write Without Response and loop-context `onWritten` delivery.
38. ✅ `phone_alert_status`: status read, ringer subscription, Control Point writes, state change, and notification.
39. ✅ `proximity`: co-located Link Loss and Tx Power services, signed Tx Power read, and persisted Alert Level write.
40. ✅ `reference_time_update`: Control Point Get/Cancel and readable Time Update State transitions.
41. ✅ `bond_management`: uint24 Feature read and Control Point opcode delivery, without performing real bond deletion.
42. ✅ `continuous_glucose_monitoring`: E2E-CRC Feature and Measurement verification plus SFLOAT/time-offset decoding.
43. ✅ `disconnect_reason`: distinct non-zero local/remote termination reasons on Peripheral and Central paths.
44. ✅ `connection_parameters`: exposed interval/latency/timeout and negotiated interval update callbacks on both peers.
45. ✅ `phy_update`: exposed TX/RX PHY and negotiated 2M PHY callbacks on both peers.
46. ✅ `service_changed`: Service Changed subscription, indication, and changed-handle-range decoding.
47. ✅ `runtime_passkey`: dynamic DisplayOnly-to-KeyboardOnly passkey relay and authenticated bonded completion.
48. ✅ `numeric_comparison`: matching six-digit values, confirmation on both sides, and authenticated bonded completion.
49. ✅ `hid_boot_protocol`: Protocol Mode switch, eight-byte Boot Input notification, and Boot Output LED write.
50. ✅ `hid_custom`: vendor Report Descriptor with Input/Output/Feature reports sharing UUID 0x2A4D; handle-based discovery, descriptor/report access, Report Reference type validation, characteristic-flag validation, feature delivery, and invalid/unknown-handle errors.
51. ✅ `beacon`: non-connectable, non-scannable marker Service UUID and manufacturer-data broadcast.
52. ✅ `persistent_subscribe`: automatic subscription restoration and notification delivery after reconnect without another `subscribe()` call.
53. ✅ `address_privacy`: Random Static advertising observed with random address type and the required leading address bits.
54. ✅ `ibeacon`: non-connectable iBeacon broadcast and complete field decoding.
55. ✅ `service_data`: multiple AD 0x16 blocks, UUID/payload enumeration, and 16-bit/full-UUID lookup equivalence.
56. ✅ `fitness_machine`: Feature and Indoor Bike Data decoding, Control Point response, Status notification, unsubscribe, and disconnect.
57. ✅ `scan_response`: passive-versus-active payload separation and merged Service UUID, Appearance, Tx Power, name, and manufacturer data.
58. ✅ `accept_list`: connection policy denial/allow, requested-timeout reporting, scan filtering, entry readback, and removal.
59. ✅ `local_identity`: local/observed address agreement, Tx Power application and advertising, and explicit disconnect-reason normalization.
60. ✅ `duplicate_uuid`: duplicate services and characteristics receive distinct handles and remain readable/subscribable by handle.
61. ✅ `directed_advertising`: directed target connection, return to normal advertising, and channel-39-only connection.
62. ✅ `multi_listener`: primary plus multiple listeners, removal of only the selected listener, unknown-ID failure, and a non-evicting four-listener limit.
63. ✅ `hid_convenience`: character/usage/layout keyboard helpers, mouse, consumer/system, gamepad helpers, invalid-character rejection, and HID Host multi-listener behavior without using raw `sendReport()`.
64. ✅ `persistent_subscription_overflow`: two peer identities fill the 16-record registry; the seventeenth successful subscription increments `droppedPersistentSubscriptionCount()` instead of silently losing observability.
65. ✅ `gatt_queue_purge`: disconnect is deferred behind an in-flight GATT operation, queued operations receive explicit failure completions, the in-flight operation completes first, events are not dropped, and reconnect/discovery still work.
66. ✅ `wifi_ble_coexistence`: on P4/C6, Wi-Fi obtains DHCP before BLE shares the Hosted transport; scan, connect, GATT read/write, subscribe, and notify work while Wi-Fi remains connected; `EspBle::end()` releases only BLE ownership and final `WiFi.STA.end()` releases the transport. S3 uses the explicit capability gate described above.
67. ✅ `rpa_bond`: enables host-based privacy on two original ESP32 boards, verifies OTA RPAs in scan and connection results, pairs and performs encrypted GATT, reboots both ends and restores IRK/LTK encryption, then deletes the restored bond and completes a fresh pairing without leaving a stale resolving-list entry.
68. ✅ `dual_host_rpa`: runs the bundled NimBLE host and custom Classic host on two original ESP32 boards, keeps the Classic HID ACL link live while pairing over host-generated RPAs, then disconnects and restores encrypted LE GATT from the bond while verifying OTA RPA address types on scan and both connection sides. It shortens each role's host timeout to two seconds, drives Classic HID reports in both directions during three consecutive RPA rotations, and verifies that preempted advertising and scanning resume after every rotation. Finite eight-second advertising and scanning remain active at three seconds but expire after their original deadline at nine seconds. It also verifies NimBLE routing of HCI LE Set Random Address and zero unknown events, queue overflows, or command-response mismatches in the broker. It reconnects bonded LE through the changed RPAs, then reboots both dual-host stacks, reconnects Classic, observes newly generated RPAs, and restores LE encryption from the persisted IRK/LTK.

The experimental `dual_host_smoke` concurrently issues Classic scan-mode changes from a separate task and NimBLE `Read RSSI` commands while Classic HID and encrypted LE GATT are connected on both boards. It verifies FIFO enqueue/physical-send equality, final RSSI completion, zero broker errors, then repeats encrypted GATT and bidirectional HID traffic after every contention cycle. `ESPBLE_DUAL_CONTENTION_CYCLES` controls the repetition. A test-only dispatch hold fills the FIFO, verifies excess rejection, then verifies GATT, HID, and lifecycle recovery after discarding the deliberately unsent commands. These commands never reach the controller and neither host receives a synthetic response. Inventories collected both while connected and after both links disconnect verify that every opcode, including conditional cleanup commands, has an explicit policy; unknown or wrong-host opcodes are rejected before physical transmission only in dual-host mode. Null and oversized HID Input/Output reports are rejected locally with `InvalidArgument`, while both connections and subsequent normal traffic remain live. Pairing first uses a wrong passkey and requires failure, no encryption, zero bonds, protected-GATT rejection, and uninterrupted Classic operation on both sides; an LE-only reconnect with the new correct passkey must then restore encryption, bonding, and GATT. The test next disconnects only Classic, requires the final asynchronous HID `onConnectionFailed` notification, verifies encrypted LE GATT remains live, and immediately reconnects Classic to the correct peer with bidirectional HID traffic. Bluedroid's public HID API cannot cancel paging, so the contract uses the backend's final OPEN result instead of pretending to implement an arbitrary timeout. The test then abruptly software-resets the peer, observes both LE and BR/EDR disconnects on the survivor, restores bonded LE encryption and Classic HID without restarting the surviving hosts, and revalidates encrypted GATT plus bidirectional HID. With the callback-target lifetime barrier enabled, the lifecycle phase covers Classic-first and BLE-first shutdown, Classic reattachment, stop/re-registration, and both destructor orders, requiring no panic, watchdog, or heap loss. Persistent-NVDS `Write Local Name` is deliberately excluded because repeated use as a stress stimulus triggers a controller assertion.

## Pass Criteria

- Test code generates every input and decides the result through serial assertions.
- Pass/fail conditions, including timeout and retry behavior, are fixed.
- Coverage includes combinations with direct implementations using the bundled BLE API, not only EspBle-to-EspBle communication.
- Items that require manual verification are not mixed into automated pass criteria.
