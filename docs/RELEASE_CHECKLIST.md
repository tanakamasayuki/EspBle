# Release Checklist

Use this checklist before releasing EspBle. The GitHub Actions workflows and `tools/` bump scripts come from the shared release toolkit and should not be edited during a normal release.

## Preflight

- `README.ja.md` / `README.md`, `docs/STATUS.ja.md`, and `docs/FEATURE_MATRIX.ja.md` match the implemented scope.
- `docs/API_DESIGN.ja.md`, `docs/HID_DEVICE_SPEC.ja.md`, and `docs/HID_HOST_SPEC.ja.md` match the public API.
- `examples/README.ja.md` / `examples/README.md` and per-example READMEs use the implemented APIs.
- No links to completed temporary plans or removed API names remain.
- `CHANGELOG.md` records all user-visible changes under `Unreleased`.

## Metadata

- `library.properties` `name`, `version`, `sentence`, `paragraph`, `architectures`, and `includes` match the public package.
- `keywords.txt` includes the main classes, report/event types, accessors, and callback/listener APIs.
- Generated `docs/BOARDS.<version>.md` / `docs/COMPATIBILITY.<version>.md` files match the release version and current example set.

## Automated Tests

Connect two ESP32-S3 boards. After library upgrades or profile changes, use `--clean` to avoid stale build caches.

```sh
cd tests
uv run --env-file .env pytest --clean
```

Compile every example for ESP32-S3:

```sh
set -euo pipefail
for sketch in $(find examples -name sketch.yaml -printf '%h\n' | sort); do
  arduino-cli compile --profile esp32s3 "$sketch"
done
```

- `compile-examples.yml` passes for every example on ESP32-S3.
- Run `board-matrix.yml` / `core-matrix.yml` manually and update the generated documents.
- Run the peer suite repeatedly immediately before release; check for flaky failures, heap loss, and leaked tasks.

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
