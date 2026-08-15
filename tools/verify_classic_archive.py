#!/usr/bin/env python3
"""Verify the checked-in Classic Bluedroid archive without regenerating it."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MANIFEST_PATH = ROOT / "src" / "esp32" / "MANIFEST.json"
PLAIN_BLUEDROID_PREFIXES = (
    "BTM_",
    "bta_",
    "btc_",
    "esp_a2d_",
    "esp_avrc_",
    "esp_bluedroid_",
    "esp_bt_gap_",
    "esp_bt_hid_device_",
    "esp_bt_hid_host_",
    "esp_hf_",
    "esp_spp_",
)


class VerificationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def run_output(command: list[str]) -> str:
    result = subprocess.run(
        command,
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise VerificationError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stderr.strip()}"
        )
    return result.stdout


def nm_symbols(path: Path, selection: str) -> set[str]:
    output = run_output(["nm", "-g", selection, "--format=posix", str(path)])
    symbols: set[str] = set()
    for line in output.splitlines():
        fields = line.split()
        if not fields or fields[0].endswith(":"):
            continue
        symbols.add(fields[0])
    return symbols


def names_sha256(names: set[str]) -> str:
    payload = "".join(f"{name}\n" for name in sorted(names)).encode()
    return sha256_bytes(payload)


def resolve_recorded_path(base: Path, value: str) -> Path:
    return (base / value).resolve()


def verify_recorded_file(base: Path, record: dict[str, object], label: str) -> None:
    path = resolve_recorded_path(base, str(record["file"]))
    require(path.is_file(), f"{label} is missing: {path}")
    actual = sha256_file(path)
    expected = str(record["sha256"])
    require(actual == expected, f"{label} SHA-256 differs: {actual} != {expected}")


def verify_archive(manifest: dict[str, object]) -> None:
    base = MANIFEST_PATH.parent
    artifact_record = manifest["artifact"]
    require(isinstance(artifact_record, dict), "artifact record must be an object")
    artifact = resolve_recorded_path(base, str(artifact_record["file"]))
    require(artifact.is_file(), f"Classic archive is missing: {artifact}")

    actual_size = artifact.stat().st_size
    expected_size = int(artifact_record["size_bytes"])
    require(
        actual_size == expected_size,
        f"archive size differs: {actual_size} != {expected_size}",
    )

    actual_sha = sha256_file(artifact)
    expected_sha = str(artifact_record["sha256"])
    require(
        actual_sha == expected_sha,
        f"archive SHA-256 differs: {actual_sha} != {expected_sha}",
    )

    members_text = run_output(["ar", "t", str(artifact)])
    members = members_text.splitlines()
    require(
        len(members) == int(artifact_record["archive_member_count"]),
        f"archive member count differs: {len(members)}",
    )
    require(
        sha256_bytes(members_text.encode())
        == str(artifact_record["archive_member_names_sha256"]),
        "archive member names or order differ from the manifest",
    )

    defined = nm_symbols(artifact, "--defined-only")
    require(
        len(defined) == int(artifact_record["global_defined_symbol_count"]),
        f"global defined symbol count differs: {len(defined)}",
    )
    require(
        names_sha256(defined)
        == str(artifact_record["global_defined_symbol_names_sha256"]),
        "global defined symbol names differ from the manifest",
    )
    prefix = str(artifact_record["defined_symbol_prefix"])
    unprefixed = sorted(name for name in defined if not name.startswith(prefix))
    require(
        not unprefixed,
        "archive has unprefixed global definitions: "
        + ", ".join(unprefixed[:20]),
    )

    required = set(manifest["required_defined_symbols"])
    missing = sorted(required - defined)
    require(not missing, "archive is missing required definitions: " + ", ".join(missing))

    undefined = nm_symbols(artifact, "--undefined-only")
    external = undefined - defined
    require(
        len(external) == int(artifact_record["external_undefined_symbol_count"]),
        f"external undefined symbol count differs: {len(external)}",
    )
    require(
        names_sha256(external)
        == str(artifact_record["external_undefined_symbol_names_sha256"]),
        "external undefined symbol names differ from the manifest",
    )
    known_internal = set(manifest["known_unprefixed_external_bluedroid_symbols"])
    require(
        known_internal <= external,
        "recorded dead BLE references no longer match the archive",
    )

    for record in manifest["build_inputs"]:
        verify_recorded_file(base, record, "build input")
    license_records = manifest["license_files"]
    for record in license_records:
        verify_recorded_file(base, record, "license file")
    recorded_licenses = {
        resolve_recorded_path(base, str(record["file"]))
        for record in license_records
    }
    actual_licenses = {
        path.resolve()
        for path in (base / "LICENSES").iterdir()
        if path.is_file()
    }
    require(
        actual_licenses == recorded_licenses,
        "LICENSES directory and manifest license inventory differ",
    )
    verify_recorded_file(base, manifest["notice"], "notice")

    inventory = manifest["license_inventory"]
    inventory_members = sum(int(item["members"]) for item in inventory.values())
    inventory_non_empty = sum(int(item["non_empty"]) for item in inventory.values())
    inventory_empty = sum(int(item["empty"]) for item in inventory.values())
    require(inventory_members == len(members), "license inventory member total differs")
    require(
        inventory_non_empty == int(artifact_record["non_empty_member_count"]),
        "license inventory non-empty total differs",
    )
    require(
        inventory_empty == int(artifact_record["empty_member_count"]),
        "license inventory empty total differs",
    )

    print(
        "Classic archive OK: "
        f"{actual_size} bytes, {len(members)} members, {len(defined)} prefixed definitions"
    )


def verify_linked_outputs(
    manifest: dict[str, object], classic_elf: Path, classic_map: Path, other_map: Path
) -> None:
    for path in (classic_elf, classic_map, other_map):
        require(path.is_file(), f"linked output is missing: {path}")

    artifact_name = str(manifest["artifact"]["file"])
    classic_map_text = classic_map.read_text(encoding="utf-8", errors="replace")
    other_map_text = other_map.read_text(encoding="utf-8", errors="replace")
    require(
        artifact_name in classic_map_text,
        "original-ESP32 map did not select the Classic archive",
    )
    require(
        artifact_name not in other_map_text,
        "non-ESP32 map unexpectedly selected the Classic archive",
    )

    linked_defined = nm_symbols(classic_elf, "--defined-only")
    linked_required = {
        "espble_bd_esp_bluedroid_attach_hci_driver",
        "espble_bd_esp_spp_register_callback",
    }
    missing = sorted(linked_required - linked_defined)
    require(
        not missing,
        "final Classic ELF is missing namespaced host symbols: "
        + ", ".join(missing),
    )

    plain = sorted(
        name
        for name in linked_defined
        if name.startswith(PLAIN_BLUEDROID_PREFIXES)
    )
    require(
        not plain,
        "final Classic ELF contains plain Bluedroid host symbols: "
        + ", ".join(plain[:20]),
    )

    namespaced_count = sum(name.startswith("espble_bd_") for name in linked_defined)
    require(namespaced_count > 0, "final Classic ELF contains no namespaced host symbols")
    print(
        "Classic link isolation OK: "
        f"{namespaced_count} namespaced definitions; non-ESP32 map excludes the archive"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--classic-elf", type=Path)
    parser.add_argument("--classic-map", type=Path)
    parser.add_argument("--other-map", type=Path)
    args = parser.parse_args()
    linked = (args.classic_elf, args.classic_map, args.other_map)
    if any(linked) and not all(linked):
        parser.error(
            "--classic-elf, --classic-map and --other-map must be supplied together"
        )
    return args


def main() -> int:
    args = parse_args()
    try:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        require(manifest.get("schema_version") == 1, "unsupported manifest schema")
        verify_archive(manifest)
        if args.classic_elf:
            verify_linked_outputs(
                manifest,
                args.classic_elf.resolve(),
                args.classic_map.resolve(),
                args.other_map.resolve(),
            )
    except (KeyError, OSError, TypeError, ValueError, VerificationError) as error:
        print(f"Classic archive verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
