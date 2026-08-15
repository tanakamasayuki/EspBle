# Peer Tests

> 日本語版: [README.ja.md](README.ja.md)

Automated Bluetooth LE tests using two ESP32-S3 boards as the baseline fixture. An ESP32-P4 + ESP32-C6 parent DUT can also use an ESP32-S3 as its wireless peer to exercise the ESP-Hosted path. No signal wiring is required between the BLE peers.

```sh
uv run --env-file .env pytest peer/
```

The profiles and environment variables reuse the existing EspUsbHost/EspUsbDevice setup:

| pytest side | profile | environment variable |
|---|---|---|
| normal parent | `s3_peer_host` | `TEST_SERIAL_PORT_S3_PEER_HOST` |
| ESP-Hosted parent | `p4_peer_host` | `TEST_SERIAL_PORT_P4_PEER_HOST` |
| original-ESP32 parent | `esp32_peer_host` | `TEST_SERIAL_PORT_ESP32_PEER_HOST` |
| second peer | `s3_peer_device` | `TEST_SERIAL_PORT_PEER_DEVICE_S3_PEER_DEVICE` |
| second peer (original ESP32) | `esp32_peer_device` | `TEST_SERIAL_PORT_PEER_DEVICE_ESP32_PEER_DEVICE` |

The profile names do not describe BLE roles. Sketches are flashed to and run on both boards, and both serial ports are observed from pytest. Within a suite the parent-side sketch is the central and the `peer_device/` sketch the peripheral; which physical board takes which side is chosen with `--profile` and `--peer-profile`.

`pytest peer/` defaults to the two-S3 fixture. The P4 fixture may remain disconnected between runs; select it explicitly for Hosted-related changes, Core/C6 firmware updates, and release candidates. See the [tests README](../README.md) and [test policy](../TEST_PLAN.md#p4c6-esp-hosted-regression) for reference wiring, commands, frequency, and known-limit exclusions.

## Original ESP32

The original ESP32 runs on the NimBLE host EspBle bundles for it (see [PLAN_ESP32.ja.md](../../docs/PLAN_ESP32.ja.md), Japanese). The two boards stay wired. pytest holds a port exclusively, so a second pytest run against the same board waits its turn; do not use `arduino-cli upload` or `esptool` directly -- they fail instead of waiting. This chip is also the only target with a Bluetooth Classic radio, so the Classic and dual-host suites run on the same pair.

```sh
# original ESP32 as the parent (central), S3 as the peer
uv run --env-file .env pytest peer/gatt_read_write/ \
  --profile esp32_peer_host \
  --peer-profile device:s3_peer_device

# original ESP32 as the peer (peripheral), S3 as the parent
uv run --env-file .env pytest peer/hid_keyboard_device/ \
  --profile s3_peer_host \
  --peer-profile device:esp32_peer_device
```

Every suite whose sketch on a given side is written with EspBle carries the matching esp32 profile. A side without one is skipped automatically when that profile is selected, and only two cases lack it:

- **The side written with the `BLE` wrapper bundled with the core.** On the original ESP32 that wrapper is Bluedroid, which cannot share one controller with the NimBLE host EspBle brings, so the build is rejected with `#error`. That is the parent side of `stack_smoke`, `advertise_payload`, `hid_keyboard_device` and `midi_device`, and the peer side of `stack_smoke` and `midi_host`. **The opposite side of each is written with EspBle and does have the profile.**
- **`phy_update`**, because the original ESP32 has a BLE 4.2 controller and no LE 2M PHY.

The reverse also exists: `rpa_bond` and the Classic and dual-host suites are original-ESP32 only and carry no S3 profile.

## Suite layout

One suite is one directory.

```text
peer/<suite>/
  <suite>.ino        parent-side sketch
  peer_device/       second-board sketch (absent for single-board suites)
  sketch.yaml        the profiles it builds for, i.e. the boards it supports
  test_<suite>.py    generates the input and decides the result
```

**What every suite covers, and what counts as a pass, is in the [test plan](../TEST_PLAN.md)** —
this file does not repeat it. When you add a suite, add it to the test plan in both languages as
well, so that every directory under `tests/peer` appears in both.

Before writing a new suite, read "Do not wait for a startup banner" and "Anchor a trailing
variable-length field" in the test plan. Both describe failures that happened on hardware.
