# Release Checklist

Use this checklist before releasing EspBle. The GitHub Actions workflows and `tools/` bump scripts come from the shared release toolkit and should not be edited during a normal release.

## Preflight

- `README.ja.md` / `README.md`, `docs/STATUS.ja.md` / `docs/STATUS.md`, and `docs/FEATURE_MATRIX.ja.md` / `docs/FEATURE_MATRIX.md` match the implemented scope.
- The Japanese and English editions of the user-facing documents are in sync (root `README`, `docs/README`, `docs/GUIDE_BLE_BASICS`, `docs/STATUS`, `docs/FEATURE_MATRIX`, `docs/RELEASE_CHECKLIST`, `examples/README` and the per-example READMEs).
- `docs/API_DESIGN.ja.md`, `docs/HID_DEVICE_SPEC.ja.md`, and `docs/HID_HOST_SPEC.ja.md` match the public API.
- `examples/README.ja.md` / `examples/README.md` and per-example READMEs use the implemented APIs.
- No links to completed temporary plans or removed API names remain.
- `CHANGELOG.md` records all user-visible changes under `Unreleased`.

## Metadata

- `library.properties` `name`, `version`, `sentence`, `paragraph`, `architectures`, and `includes` match the public package.
- `keywords.txt` includes the main classes, report/event types, accessors, and callback/listener APIs.
- Generated `docs/BOARDS.<version>.md` / `docs/COMPATIBILITY.<version>.md` files match the release version and current example set.

## Automated Tests

First connect two ESP32-S3 boards and run the full baseline regression. After library upgrades or profile changes, use `--clean` to avoid stale build caches.

```sh
cd tests
uv run --env-file .env pytest --clean
```

Then connect the P4+C6 fixture and its S3 peer and run the representative ESP-Hosted suite. The fixture need not remain connected between runs, but it is mandatory for a release candidate. Use an ESP32-P4-Function-EV-Board or a fixture with the generic `esp32p4` variant's standard SDIO wiring as the reference; record the variant or pin override when using custom wiring.

```sh
uv run --env-file .env pytest --clean \
  peer/stack_smoke/ peer/connect_disconnect/ peer/gatt_read_write/ \
  peer/notify_indicate/ peer/mtu/ peer/wifi_ble_coexistence/ \
  --profile p4_peer_host \
  --peer-profile device:s3_peer_device
```

See the [ESP-Hosted setup guide (Japanese)](ESP_HOSTED_SETUP.ja.md) for fixture and pin requirements, the [test policy](../tests/TEST_PLAN.ja.md#p4c6-esp-hosted回帰) for frequency and pass criteria, and the [known limitations (Japanese)](ESP_HOSTED_LIMITATIONS.ja.md) for current exclusions. Security and repeated full initialization/deinitialization cases affected by those limitations are not current release gates; re-run them after any Core or C6 firmware update to check whether the limitation has been resolved.

Compile every example for ESP32-S3:

```sh
set -euo pipefail
for sketch in $(find examples -name sketch.yaml -printf '%h\n' | sort); do
  arduino-cli compile --profile esp32s3 "$sketch"
done
```

- `compile-examples.yml` passes for every example on ESP32-S3.
- Run `board-matrix.yml` / `core-matrix.yml` manually and update the generated documents.
- Run the S3 peer suite repeatedly immediately before release; check for flaky failures, heap loss, and leaked tasks. Pass the P4 representative suite at least once on the final candidate.

## Manual Interoperability

Record the date and OS/device version for each result.

- Connect the HID Device to at least two external Host implementations (for example Android and Linux); check keyboard, mouse, consumer control, and reconnection.
- Connect the HID Host to at least one commercial BLE keyboard; check input, modifiers, LEDs, disconnect release, and bonded reconnection.
- Verify Just Works and static-passkey pairing with an external BLE implementation.
- Verify basic scan, GATT read/write, and notify flows with a phone or desktop BLE tool.

## Final Checks and Release

- Run `git diff --check` and a link/reference audit; exclude build artifacts, caches, and local-profile-only changes.
- Preview the version change with the bump script.
- Use the release workflow to create the version update, release branch, tag, and GitHub release.
- After publication, verify the Arduino Library Manager version and compile the minimal example from the published package.
