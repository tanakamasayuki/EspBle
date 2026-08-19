# Release Checklist

> 日本語版: [RELEASE_CHECKLIST.ja.md](RELEASE_CHECKLIST.ja.md)

Use this checklist before releasing EspBle. The GitHub Actions workflows and `tools/` bump scripts come from the shared release toolkit and should not be edited during a normal release.

## Preflight

- `README.ja.md` / `README.md`, `docs/STATUS.ja.md` / `docs/STATUS.md`, and `docs/FEATURE_MATRIX.ja.md` / `docs/FEATURE_MATRIX.md` match the implemented scope.
- The Japanese and English editions of the user-facing documents are in sync (root `README`, `docs/README`, `docs/GUIDE_BLE_BASICS`, `docs/GUIDE_CLASSIC_BASICS`, `docs/GUIDE_ADVANCED`, `docs/GUIDE_MIGRATION`, `docs/GUIDE_HID_DESCRIPTORS`, `docs/API_DESIGN`, `docs/CLASSIC_VS_BLE`, `docs/STATUS`, `docs/FEATURE_MATRIX`, `docs/RELEASE_CHECKLIST`, `tests/TEST_PLAN`, `examples/README`, and the per-example READMEs).
- `docs/API_DESIGN.md` / `docs/API_DESIGN.ja.md`, `docs/HID_DEVICE_SPEC.ja.md`, and `docs/HID_HOST_SPEC.ja.md` match the public API.
- `examples/README.ja.md` / `examples/README.md` and per-example READMEs use the implemented APIs.
- No links to completed temporary plans or removed API names remain.
- `CHANGELOG.md` records all user-visible changes under `Unreleased`.

## Metadata

- `library.properties` `name`, `version`, `sentence`, `paragraph`, `architectures`, and `includes` match the public package.
- `keywords.txt` includes the main classes, report/event types, accessors, and callback/listener APIs.
- Generated `docs/BOARDS.<version>.md` / `docs/COMPATIBILITY.<version>.md` files match the release version and current example set.
- User-facing Classic documentation consistently states the measured support range (core 3.2.0 and newer, HFP audio 3.3.8 and newer; the archive is built with ESP-IDF 5.5.5 / GCC 14.2.0 and ships its contract headers).
- The release zip contains the root `THIRD_PARTY_NOTICES.md`, Classic `NOTICE` / `MANIFEST.json` / `LICENSES/`, and NimBLE `LICENSE` / `NOTICE`, matching the manifest's license inventory.
- If Classic is in scope, regenerate its archive temporarily from clean ESP-IDF v5.5.5 / GCC 14.2.0 inputs, verify its SHA-256 matches the stored `libespble_bluedroid_classic.a` and the required prefixed symbols are present, link a final ESP32 consumer, and prove that other SoCs do not link it. The authoritative procedure is [CLASSIC_HOST_BUILD.ja.md](CLASSIC_HOST_BUILD.ja.md) (Japanese).

## Automated Tests

First connect two ESP32-S3 boards and run the full baseline regression. After library upgrades or profile changes, use `--clean` to avoid stale build caches.

```sh
cd tests
uv run --env-file .env pytest --clean
```

`rpa_bond` is original-ESP32 only, for the reason recorded in the [test plan](../tests/TEST_PLAN.md). Without `--profile` each sketch uses its own `default_profile`, so this run drives that suite on the original-ESP32 pair rather than the S3 boards; with the pair unplugged it skips for the missing port.

Next, sweep the two original-ESP32 boards (ports come from `TEST_SERIAL_PORT_ESP32_PEER_HOST` and `TEST_SERIAL_PORT_PEER_DEVICE_ESP32_PEER_DEVICE` in `.env`) in both roles. That chip runs the NimBLE host **EspBle bundles** (`src/nimble_esp32/`) rather than the core's, so any release that touches `src/` must run it. Each sweep takes about an hour.

```sh
# original ESP32 as the parent (central)
uv run --env-file .env pytest --clean peer/ \
  --ignore=peer/classic_hid_profiles --ignore=peer/classic_a2dp_sink_profile \
  --ignore=peer/classic_radio_settings \
  --profile esp32_peer_host --peer-profile device:s3_peer_device

# original ESP32 as the peer (peripheral)
uv run --env-file .env pytest --clean peer/ \
  --ignore=peer/classic_hid_profiles --ignore=peer/classic_a2dp_sink_profile \
  --ignore=peer/classic_radio_settings \
  --profile s3_peer_host --peer-profile device:esp32_peer_device
```

A suite without an esp32 profile for that role skips itself. What skips is the side written
against the core's bundled `BLE` wrapper -- unusable on the original ESP32, where that wrapper is
Bluedroid -- and `phy_update`, which requires the 2M PHY. The three `--ignore`d suites have no
peer sketch, so they error as "unknown peer" instead of skipping; they run in the Classic
single-board command below.

For a documentation-only release that does not touch `src/`, the representative smoke set is enough (about 15 minutes for both roles).

```sh
uv run --env-file .env pytest --clean \
  peer/gatt_read_write/ peer/security_bond/ peer/hid_keyboard_host/ \
  peer/mtu/ peer/connection_parameters/ \
  --profile esp32_peer_host --peer-profile device:s3_peer_device
```

The exclusions and the frequency rule are in the [test plan](../tests/TEST_PLAN.md#original-esp32-regression); the policy and verification record are in the Japanese [original-ESP32 plan](PLAN_ESP32.ja.md). pytest holds a port exclusively, so a second pytest run against the same board waits its turn. Do not use `arduino-cli upload` or `esptool` directly -- they fail instead of waiting.

If Classic is in scope, add the Classic-only and dual-host regressions on the same two original-ESP32 boards:

```sh
# Suites with no peer sketch run on their own: passing --peer-profile to them is
# rejected as an unknown peer.
uv run --env-file .env pytest --clean -s \
  peer/classic_hid_profiles/ peer/classic_a2dp_sink_profile/ \
  peer/classic_radio_settings/ \
  --profile esp32_peer_host

uv run --env-file .env pytest --clean -s \
  peer/classic_inquiry/ peer/classic_pairing/ \
  peer/classic_spp_exclusive/ peer/classic_spp_stream/ \
  peer/classic_core_host_spp/ \
  peer/classic_hid_api/ peer/classic_hid_control/ \
  peer/classic_hid_report/ peer/classic_hid_gamepad/ \
  peer/classic_a2dp_media/ peer/classic_hfp_client/ \
  peer/classic_hfp_cvsd/ \
  peer/dual_host_smoke/ peer/dual_host_rpa/ peer/dual_host_hfp/ \
  peer/dual_host_a2dp/ \
  --profile esp32_peer_host --peer-profile device:esp32_peer_device
```

Passing `--peer-profile device:...` to a suite with no peer sketch, such as
`classic_hid_profiles`, makes pytest reject it as an unknown peer. Do not merge
the two commands above into one.

Complete the hours-long soak under the conditions in the Japanese
[Classic handoff](HANDOFF_ESP32_CLASSIC.ja.md) before code freeze. Re-run the normal regression above on the release candidate and retain its soak logs, heap samples and broker diagnostics in the technical-validation record.

Then connect the P4+C6 fixture and its S3 peer and run the representative ESP-Hosted suite. The fixture need not remain connected between runs, but it is mandatory for a release candidate. Use an ESP32-P4-Function-EV-Board or a fixture with the generic `esp32p4` variant's standard SDIO wiring as the reference; record the variant or pin override when using custom wiring.

```sh
uv run --env-file .env pytest --clean \
  peer/stack_smoke/ peer/connect_disconnect/ peer/gatt_read_write/ \
  peer/notify_indicate/ peer/mtu/ peer/wifi_ble_coexistence/ \
  --profile p4_peer_host \
  --peer-profile device:s3_peer_device
```

See the [ESP-Hosted setup guide (Japanese)](ESP_HOSTED_SETUP.ja.md) for fixture and pin requirements, the [test policy](../tests/TEST_PLAN.md#p4c6-esp-hosted-regression) for frequency and pass criteria, and the [known limitations (Japanese)](ESP_HOSTED_LIMITATIONS.ja.md) for current exclusions. Security and repeated full initialization/deinitialization cases affected by those limitations are not current release gates; re-run them after any Core or C6 firmware update to check whether the limitation has been resolved.

Core-version gate for the original ESP32 (no hardware). The supported range is
3.2.0 through 3.3.11; the vendored contract headers keep it from drifting with the
core version, but every release re-measures it. All cells passing is the pass
criterion. The measurement record is the Japanese
[core-version test plan](PLAN_CORE_VERSION_MATRIX.ja.md).

```sh
python3 tools/version_matrix.py \
  --core-versions 3.2.0,3.2.1,3.3.0,3.3.11 --targets esp32 \
  --examples CompileSmoke,Gap/Connect,Security/StaticPasskeyServer,Classic/SppServer,Classic/HidKeyboard,Classic/A2dpSinkAvrcp,Classic/HfpClientRaw \
  --output /tmp/esp32_core_matrix.md
```

The original-ESP32 column of `docs/COMPATIBILITY.<version>.md` still holds results from
before Classic entered `src/`. Run `core-matrix.yml` at release time and confirm that
column shows ✅ from 3.2.0 up.

Compile every example. The Classic examples are original-ESP32-only and carry no
`esp32s3` profile, so fall back to each sketch's `default_profile`, the same way
`compile-examples.yml` does:

```sh
set -uo pipefail
for sketch in $(find examples -name sketch.yaml -printf '%h\n' | sort); do
  profile=esp32s3
  grep -q "^  esp32s3:" "$sketch/sketch.yaml" ||
    profile=$(grep -oP '^default_profile:\s*\K\S+' "$sketch/sketch.yaml")
  arduino-cli compile --profile "$profile" "$sketch"
done
```

- Run the S3 peer suite repeatedly immediately before release; check for flaky failures, heap loss, and leaked tasks. Pass the P4 representative suite at least once on the final candidate.

The local loop above covers esp32 and esp32s3. Compiling every example across the whole release board set belongs to `compile-examples.yml`, together with `board-matrix.yml` and `core-matrix.yml`; see "Workflows" below.

## Manual Interoperability

Record the date and OS/device version for each result.

- Connect the HID Device to at least two external Host implementations (for example Android and Linux); check keyboard, mouse, consumer control, and reconnection.
- Connect the HID Host to at least one commercial BLE keyboard; check input, modifiers, LEDs, disconnect release, and bonded reconnection.
- Verify Just Works and static-passkey pairing with an external BLE implementation.
- Verify basic scan, GATT read/write, and notify flows with a phone or desktop BLE tool.

## Final Checks

- Run `python tools/verify_classic_archive.py`; the archive, manifest, licenses, build inputs and symbol inventory must match.
- Run `python tools/release_hooks/pre_bump.py`; every Classic host call must be namespaced, the ESP32 final ELF must use only the bundled host, and the non-ESP32 map must exclude it.
- Run `git diff --check` and a link/reference audit; exclude build artifacts, caches, and local-profile-only changes.

## Workflows

These run in GitHub Actions. Running them locally first would not change the result, so they are release-time steps rather than gates; record the outcome.

- `board-matrix.yml`: regenerate the board set and settle `docs/BOARDS.<version>.md`.
- `core-matrix.yml`: recheck the supported Arduino-ESP32 versions and settle `docs/COMPATIBILITY.<version>.md`.
- `compile-examples.yml`: run the Classic archive integrity, source namespace and final-link gates, then compile every example across the release board set.
- `release.yml`: run the same Classic gate through the pre-bump hook, preview the version change, then create the version update, CHANGELOG, release branch, tag, and GitHub release.
- After publication, verify the Arduino Library Manager version and compile the minimal example from the published package.
