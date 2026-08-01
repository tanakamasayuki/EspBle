# Tests

> 日本語版: [README.ja.md](README.ja.md)

EspBle tests use pytest-embedded with the Arduino CLI backend. See the [test plan](TEST_PLAN.ja.md) (Japanese) for the coverage policy.

```text
unit/   pure C++/data-conversion tests that run on the host (no boards required)
peer/   hardware BLE tests for the S3 baseline and P4+C6 ESP-Hosted paths
```

| Fixture | Purpose | Normal use |
|---|---|---|
| ESP32-S3 + ESP32-S3 | Full functional baseline | Keep connected if practical; this is the default for `pytest peer/` |
| ESP32-P4 + ESP32-C6, with an ESP32-S3 peer | SDIO/ESP-Hosted/C6-controller and Wi-Fi/BLE coexistence regression | One fixture connected on demand; select the P4 profile explicitly |

Compiling for P4 does not exercise the ESP-Hosted transport, and S3-only tests cannot detect Hosted initialization or SDIO failures. P4 hardware testing is therefore required, but the fixture does not need to remain connected. Run it for Hosted-related changes, after an Arduino-ESP32 Core or C6 firmware update, and for every release candidate. See the [test plan](TEST_PLAN.ja.md#p4c6-esp-hosted回帰) for the detailed policy, the [ESP-Hosted setup guide (Japanese)](../docs/ESP_HOSTED_SETUP.ja.md) for preparation, and the [known limitations (Japanese)](../docs/ESP_HOSTED_LIMITATIONS.ja.md) for current exclusions.

Example build regressions are caught by GitHub Actions (`.github/workflows/compile-examples.yml`), which compiles every example with the esp32s3 profile. Interoperability with OSes and commercial BLE devices is covered by manual tests.

## Setup

```sh
cp .env.example .env
uv sync
```

`.env` uses the same profile-derived variable names as the existing EspUsbHost/EspUsbDevice environment:

```dotenv
TEST_SERIAL_PORT_S3_PEER_HOST=/dev/ttyUSB0
TEST_SERIAL_PORT_PEER_DEVICE_S3_PEER_DEVICE=/dev/ttyUSB1
TEST_SERIAL_PORT_P4_PEER_HOST=/dev/ttyUSB2
TEST_WIFI_SSID=example-test-ssid
TEST_WIFI_PASSWORD=example-test-password
```

`host` and `device` identify the parent side and the second peer on pytest-embedded-cli; they do not describe BLE central/peripheral roles. Sketches are flashed to and run on both boards, and both serial ports are observed from pytest. The initial tests fix the parent side as central and the peer side as peripheral.

## Running

```sh
uv run --env-file .env pytest          # unit + peer
uv run --env-file .env pytest unit/
uv run --env-file .env pytest peer/
uv run --env-file .env pytest peer/stack_smoke/
```

The recommended reference fixture is an Espressif ESP32-P4-Function-EV-Board with its onboard C6, or a P4+C6 fixture using the same standard SDIO wiring expected by Arduino-ESP32's generic `esp32p4` variant. This gives contributors a shared configuration without a board-specific pin override. Tab5 and custom wiring are also supported, but require the matching board variant or `hostedSetPins()` before `ble.begin()`. See [SDIO pin selection and override (Japanese)](../docs/ESP_HOSTED_SETUP.ja.md#sdio-pinの選択と上書き).

Run a quick P4-to-S3 smoke test using the ports from `.env`:

```sh
uv run --env-file .env pytest peer/connect_disconnect/ \
  --profile p4_peer_host \
  --peer-profile device:s3_peer_device
```

Run the representative P4 release suite with:

```sh
uv run --env-file .env pytest \
  peer/stack_smoke/ peer/connect_disconnect/ peer/gatt_read_write/ \
  peer/notify_indicate/ peer/mtu/ peer/wifi_ble_coexistence/ \
  --profile p4_peer_host \
  --peer-profile device:s3_peer_device
```

The coexistence test can also be run alone:

```sh
uv run --env-file .env pytest peer/wifi_ble_coexistence/
```

The Wi-Fi values are passed as compile-time defines and may appear in verbose
Arduino CLI command output. Use credentials dedicated to a disposable test AP.

Security and repeated full initialization/deinitialization cases affected by the current Core/ESP-Hosted known limitations are not mandatory pass criteria for the representative P4 suite. Re-run them whenever the upstream versions change to determine whether those limitations have been resolved.
