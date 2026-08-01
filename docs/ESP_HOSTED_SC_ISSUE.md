# GitHub issue draft: ESP32-P4 NimBLE Secure Connections

投稿先: <https://github.com/espressif/arduino-esp32/issues/new/choose>

Title:

```text
ESP32-P4 NimBLE Secure Connections fails because IDF 5.5.5 produces an incorrect P-256 ECDH result
```

Body:

---

### Board

ESP32-P4 with an ESP32-C6 co-processor (ESP-Hosted)

### Device Description

ESP32-P4 runs the NimBLE host. ESP32-C6 runs the BLE controller through
ESP-Hosted SDIO/VHCI. An ESP32-S3 is used as the peripheral peer.

### Hardware Configuration

- ESP32-P4 serial port: `/dev/ttyUSB2`
- ESP32-C6 connected through 4-bit SDIO using the board's Hosted configuration
- ESP32-S3 peer connected separately over USB

### Version

Arduino-ESP32 3.3.11

### IDE Name

arduino-cli 1.3.1

### Operating System

Linux

### Flash frequency

Default

### PSRAM enabled

Default board configuration

### Upload speed

921600

### Description

LE Secure Connections pairing always fails with Security Manager error `0x0b`
(DHKey check failure) when the NimBLE host runs on ESP32-P4 with
Arduino-ESP32 3.3.11. This is not caused by corruption in the ESP-Hosted HCI
path: the first divergent value is the P-256 ECDH shared secret calculated on
the P4.

Arduino-ESP32 3.3.11 bundles ESP-IDF 5.5.5 libraries built from IDF commit
`129cd0d2` (`esp32-arduino-libs-idf-release_v5.5-129cd0d2-v4`). That revision
predates the following fix already present on ESP-IDF `release/v5.5`:

<https://github.com/espressif/esp-idf/commit/9fd7cb7e606a06111e1b14be7f4e00d77d9cf3dd>

The upstream commit is titled:

```text
fix(nimble): Fix ECC HW byte-order and dropped SOC_ESP_NIMBLE_CONTROLLER
```

In particular, it changes TinyCrypt so that the ECC peripheral receives a
canonical scalar and keeps the regularized one-bit-longer scalar on the
software ladder. It also fixes the conversion between TinyCrypt's native word
representation and the ECC peripheral's little-endian byte representation.

### Reproduction

The reproducible central/peripheral sketches and pytest are here:

<https://github.com/tanakamasayuki/EspBle/tree/cceb468/tests/peer/security_bond>

Run from `tests/`:

```sh
uv run --env-file .env pytest peer/security_bond/ \
  --profile p4_peer_host --port /dev/ttyUSB2 \
  --peer-profile device:s3_peer_device -vv
```

ESP-Hosted Host and Slave were both updated to 2.12.11 before reproducing:

```text
ESP_HOSTED_VERSION host=2.12.11 slave=2.12.11 target=esp32c6
```

The unmodified build fails every time:

```text
P4 central:    backend status 1291 (0x50b)
S3 peripheral: backend status 1035 (0x40b)
```

Both statuses contain SM reason `0x0b`.

### Root-cause isolation

I temporarily wrapped the NimBLE Security Manager boundaries on both boards
and logged the complete SMP Public Key, Random, and DHKey Check values plus the
inputs and result of `ble_sm_alg_gen_dhkey()`, `f5()`, and `f6()`.

The following were identical on both sides:

- pairing request and response;
- each 64-byte P-256 public key before transmission and after reception;
- both 16-byte random values;
- initiator/responder address types and addresses;
- IO capability bytes;
- the transmitted and received 16-byte DHKey Check.

However, the ECDH shared secrets differed. One captured run produced:

```text
S3: 07be553b3786952bc6dae6760dcc172caa999a17866368f4b0112bb86b674231
P4: 2494d0331e577f4f7971618cedaccb51558c3f0c22b70efab7233d1408f8903e
```

For a second captured key pair, an independent P-256 calculation showed:

```text
expected: e7d6179db57f7433c432d2b4a8f7c43664df58637e3de6a3c0a1b6bf6792a53a
S3:      e7d6179db57f7433c432d2b4a8f7c43664df58637e3de6a3c0a1b6bf6792a53a
P4:      11797fdfe3c504429b9717e1cd86b7726e85be1b536e0086777c5df9fec10fb3
```

Both generated public keys matched their private keys and were on P-256, so
the failure is specifically the P4 shared-secret multiplication, not key
generation or Hosted packet transport.

As a final confirmation, I used a diagnostic wrapper on P4 that passes the
original canonical private scalar and aligned little-endian point buffers to
`esp_tinycrypt_calc_ecc_mult()`, instead of the regularized scalar used by the
bundled `EccPoint_mult()` hardware path. With that change:

```text
P4 DHKey == S3 DHKey
P4 f5 MacKey == S3 f5 MacKey
both f6 DHKey Checks match
CENTRAL_SECURITY success=1 encrypted=1 bonded=1 key=16
PERIPHERAL_SECURITY success=1 encrypted=1 bonded=1 key=16
encrypted GATT read/write: pass
```

S3/S3 Secure Connections also passes without any diagnostic change.

### Expected Behavior

ESP32-P4 should calculate the same P-256 ECDH shared secret as the peer and
complete LE Secure Connections pairing, bonding, and encrypted GATT access.

### Request

Please update the prebuilt ESP32-P4 libraries used by Arduino-ESP32 to an
ESP-IDF `release/v5.5` revision containing
`9fd7cb7e606a06111e1b14be7f4e00d77d9cf3dd`, or backport that fix to the IDF
revision used for the next Arduino-ESP32 3.3.x release.

I can retest the original pairing/bond/reconnect pytest on P4/C6 hardware with
a prerelease library archive.

---
