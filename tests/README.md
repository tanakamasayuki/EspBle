# Tests

> 日本語版: [README.ja.md](README.ja.md)

EspBle tests use pytest-embedded with the Arduino CLI backend. See the [test plan](TEST_PLAN.ja.md) (Japanese) for the coverage policy.

```text
unit/   pure C++/data-conversion tests that run on the host (no boards required)
peer/   automated BLE tests using two ESP32-S3 boards
```

Example build regressions are caught by GitHub Actions (`.github/workflows/compile-examples.yml`), which compiles every example with the esp32s3 profile. Interoperability with OSes and commercial BLE devices is covered by manual tests before the first release.

## Setup

```sh
cp .env.example .env
uv sync
```

`.env` uses the same profile-derived variable names as the existing EspUsbHost/EspUsbDevice environment:

```dotenv
TEST_SERIAL_PORT_S3_PEER_HOST=/dev/ttyUSB0
TEST_SERIAL_PORT_PEER_DEVICE_S3_PEER_DEVICE=/dev/ttyUSB1
```

`host` and `device` identify the parent side and the second peer on pytest-embedded-cli; they do not describe BLE central/peripheral roles. Sketches are flashed to and run on both boards, and both serial ports are observed from pytest. The initial tests fix the parent side as central and the peer side as peripheral.

## Running

```sh
uv run --env-file .env pytest          # unit + peer
uv run --env-file .env pytest unit/
uv run --env-file .env pytest peer/
uv run --env-file .env pytest peer/stack_smoke/
```
