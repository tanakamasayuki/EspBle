# GitHub issue draft: ESP-Hosted Secure Connections

投稿先: <https://github.com/espressif/esp-hosted-mcu/issues/new>

Title:

```text
ESP32-P4 + ESP32-C6 Hosted BLE Secure Connections fails with DHKey check failure
```

Body:

---

### Checklist

- [x] Checked the issue tracker for similar issues.
- [x] Verified that the ESP-Hosted host and slave versions match.
- [x] Reproduced with a minimal P4/C6 central and S3 peripheral peer test.
- [ ] Runtime-tested ESP-Hosted 2.12.12 (Arduino-ESP32 3.3.11 currently bundles 2.12.11).

### Summary

LE Secure Connections pairing always fails with DHKey check failure when the
NimBLE host runs on ESP32-P4 and the controller runs on ESP32-C6 through
ESP-Hosted SDIO/VHCI. The same application and test pass when both peers use
local ESP32-S3 controllers.

### Environment

- Host MCU: ESP32-P4
- Controller/co-processor: ESP32-C6 over 4-bit SDIO/VHCI
- Peer: ESP32-S3
- Arduino-ESP32: 3.3.11 (ESP-IDF 5.5.5)
- ESP-Hosted host: 2.12.11
- ESP-Hosted slave: 2.12.11
- esp-hosted-mcu component commit bundled by the Core:
  `da7412f9b5a31b54bd57acb140247e95d54c7eed`
- NimBLE Security Manager: bonding enabled, MITM disabled,
  `ble_hs_cfg.sm_sc = 1`

The C6 initially had slave 2.3.2. It was updated and activated successfully,
and the runtime log confirms matching versions:

```text
ESP_HOSTED_VERSION host=2.12.11 slave=2.12.11 target=esp32c6
```

### Expected behavior

The P4/C6 central and S3 peripheral complete LE Secure Connections pairing,
store a bond, perform encrypted GATT operations, disconnect, and reconnect
using the bond. This exact test succeeds with an S3 central and S3 peripheral.

### Actual behavior

Initial pairing fails every time:

```text
P4 central:    backend status 1291 (0x50b)
S3 peripheral: backend status 1035 (0x40b)
```

Both statuses contain Security Manager error `0x0b`, DHKey check failure.
Updating the C6 slave from 2.3.2 to matching 2.12.11 did not change the result.

### Reproduction

The complete reproducible test and both Arduino sketches are here:

<https://github.com/tanakamasayuki/EspBle/tree/cceb468/tests/peer/security_bond>

Run from `tests/`, with the P4 on the DUT port and an S3 configured as the
`device` peer:

```sh
uv run --env-file .env pytest peer/security_bond/ \
  --profile p4_peer_host --port /dev/ttyUSB2 \
  --peer-profile device:s3_peer_device -vv
```

The test performs these steps:

1. Delete bonds on both devices.
2. Scan and connect from P4/C6 central to S3 peripheral.
3. Start pairing on connect with bonding and Secure Connections enabled.
4. Wait for security, then exercise encrypted GATT read/write.
5. Disconnect and reconnect to verify the bond.

It fails at step 3 before encrypted GATT begins.

### Isolation and attempted workarounds

| Configuration | Result |
| --- | --- |
| S3 central + S3 peripheral, Secure Connections | Pass |
| P4/C6 central + S3 peripheral, Secure Connections | DHKey check failure |
| P4/C6 central + S3 peripheral, P4 forced to Legacy pairing | Initial pairing, bond storage, and encrypted GATT pass |

Legacy pairing is not an acceptable transparent workaround because it is a
security downgrade. It also reveals a second Hosted-specific problem on bonded
reconnect: the S3 reports successful encryption, while the P4 receives no
security-change event. Polling `ble_gap_conn_find()` on the P4 eventually shows
`encrypted=1`, but still reports `bonded=0` and `key_size=0`, so the application
cannot reconstruct a trustworthy security result.

### Source check

The HCI paths reviewed were:

- `host/drivers/bt/vhci_drv.c`
- `slave/main/slave_bt.c`

There is no change to these files between the Arduino-bundled component commit
above and current esp-hosted-mcu main / release 2.12.12. Therefore the newer
release contains no apparent HCI-path change for this failure, although I have
not runtime-tested 2.12.12 because Arduino-ESP32 3.3.11 provides prebuilt
2.12.11 P4 libraries.

### Request

Please investigate the Hosted HCI path for the LE P-256 public-key / DHKey
commands and events, and the missing encryption-change state on bonded
reconnect. I can provide additional HCI hex logging or run a proposed patch on
the P4/C6 hardware if you indicate the preferred logging configuration.

---
