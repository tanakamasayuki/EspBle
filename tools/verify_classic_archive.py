#!/usr/bin/env python3
"""Verify the checked-in Classic Bluedroid archive without regenerating it."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MANIFEST_PATH = ROOT / "src" / "esp32" / "MANIFEST.json"
EXPECTED_ARTIFACT = {
    "file": "libespble_bluedroid_classic.a",
    "type": "static-library",
    "target": "esp32",
    "architecture": "elf32-xtensa-le",
    "defined_symbol_prefix": "espble_bd_",
}
EXPECTED_UPSTREAM = {
    "name": "ESP-IDF",
    "repository": "https://github.com/espressif/esp-idf",
    "tag": "v5.5.5",
    "commit": "b774170ff46c393eeb5e495ea37936038d3f4f4f",
    "component": "components/bt",
    "tinycrypt_version": "0.2.8",
}
EXPECTED_ABI = {
    "arduino_esp32": "3.3.11",
    "esp_idf": "5.5.5",
    "compiler": "xtensa-esp32-elf-gcc",
    "compiler_version": "14.2.0",
    "compiler_release": "esp-14.2.0_20260121",
    "arduino_tool_package": "esp-x32@2601",
}
EXPECTED_BUILD_INPUT_FILES = {
    "../../tools/build_classic_bluedroid_host.sh",
    "../../tools/classic_bluedroid_host/CMakeLists.txt",
    "../../tools/classic_bluedroid_host/sdkconfig.defaults",
    "../../tools/classic_bluedroid_host/main/CMakeLists.txt",
    "../../tools/classic_bluedroid_host/main/link_check.c",
}
# The Bluedroid API headers the archive was built against, vendored by
# tools/vendor_classic_contract.py so the compile-time contract cannot drift
# with the Arduino-ESP32 core version. Regenerating the archive must refresh
# these together with it.
EXPECTED_CONTRACT_HEADER_FILES = {
    "include/esp_a2dp_api.h",
    "include/esp_a2dp_legacy_api.h",
    "include/esp_avrc_api.h",
    "include/esp_bluedroid_hci.h",
    "include/esp_bt_defs.h",
    "include/esp_bt_main.h",
    "include/esp_gap_bt_api.h",
    "include/esp_hf_ag_api.h",
    "include/esp_hf_ag_legacy_api.h",
    "include/esp_hf_client_api.h",
    "include/esp_hf_client_legacy_api.h",
    "include/esp_hf_defs.h",
    "include/esp_hidd_api.h",
    "include/esp_hidh_api.h",
    "include/esp_spp_api.h",
}
EXPECTED_KCONFIG = {
    "CONFIG_BT_CONTROLLER_DISABLED": True,
    "CONFIG_BT_BLUEDROID_ENABLED": True,
    "CONFIG_BT_CLASSIC_ENABLED": True,
    "CONFIG_BT_BLE_ENABLED": False,
    "CONFIG_BT_SMP_CRYPTO_STACK_TINYCRYPT": True,
}
EXPECTED_POST_PROCESSING = [
    "prefix every global defined symbol with espble_bd_",
    "strip debug information",
]
EXPECTED_LICENSE_INVENTORY = {
    "apache_explicit": (269, 196, 73),
    "apache_repository_root": (5, 3, 2),
    "tinycrypt_intel": (10, 10, 0),
    "tinycrypt_intel_and_mackay": (4, 3, 1),
    "tinycrypt_chris_morrison": (1, 1, 0),
    "brian_gladman_aes": (1, 0, 1),
}
EXPECTED_LICENSE_RECORDS = {
    "LICENSES/Apache-2.0.txt": {
        "upstream_file": "LICENSE",
    },
    "LICENSES/TinyCrypt.txt": {
        "upstream_file": "components/bt/common/tinycrypt/LICENSE",
    },
    "LICENSES/Chris-Morrison-BSD-2-Clause.txt": {
        "upstream_file": "components/bt/common/tinycrypt/src/ctr_prng.c",
        "upstream_source_sha256": (
            "074473691632ef9bea200247c770771efefd6b742ccc6ed0a23aad086bb3350c"
        ),
    },
    "LICENSES/Brian-Gladman-AES.txt": {
        "upstream_file": "components/bt/host/bluedroid/stack/smp/aes.c",
        "upstream_source_sha256": (
            "a47785e107f4f5eade7c662664e4229cc5f26802d9de87ee6972fb1d2c8f60cd"
        ),
    },
}
PLAIN_BLUEDROID_PREFIXES = (
    "BTM_",
    "bta_",
    "btc_",
    "esp_a2d_",
    "esp_avrc_",
    "esp_ble_",
    "esp_bluedroid_",
    "esp_bt_gap_",
    "esp_bt_hid_device_",
    "esp_bt_hid_host_",
    "esp_hf_",
    "esp_spp_",
)
PLAIN_BLUEDROID_EXCEPTIONS = {
    # These two definitions come from the original ESP32 controller archive
    # (libbtdm_app.a), not from the Arduino Core's Bluedroid host.
    "esp_ble_disable_adv_delay",
    "esp_ble_enable_scan_forever",
}


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


def archive_payload_counts(artifact: Path) -> tuple[int, int]:
    """Count members with allocatable code/data and metadata-only members."""
    output = run_output(["size", "-A", str(artifact)])
    payload_prefixes = (".text", ".data", ".bss", ".rodata", ".literal")
    member_count = 0
    non_empty_count = 0
    current_has_payload = False
    in_member = False

    for line in output.splitlines():
        if "(ex " in line and line.endswith("):"):
            if in_member:
                non_empty_count += int(current_has_payload)
            member_count += 1
            current_has_payload = False
            in_member = True
            continue
        if not in_member:
            continue
        match = re.match(r"^(\S+)\s+(\d+)\s+", line)
        if match and match.group(1).startswith(payload_prefixes):
            current_has_payload |= int(match.group(2)) > 0

    if in_member:
        non_empty_count += int(current_has_payload)
    return non_empty_count, member_count - non_empty_count


def verify_manifest_contract(manifest: dict[str, object]) -> None:
    artifact = manifest["artifact"]
    require(isinstance(artifact, dict), "artifact record must be an object")
    for key, expected in EXPECTED_ARTIFACT.items():
        require(artifact.get(key) == expected, f"artifact {key} is not {expected!r}")
    require(manifest["upstream"] == EXPECTED_UPSTREAM, "upstream pin differs")
    require(manifest["abi"] == EXPECTED_ABI, "Classic ABI contract differs")
    require(
        manifest["effective_kconfig"] == EXPECTED_KCONFIG,
        "Classic effective Kconfig contract differs",
    )
    require(
        manifest["post_processing"] == EXPECTED_POST_PROCESSING,
        "archive post-processing contract differs",
    )

    build_inputs = manifest["build_inputs"]
    require(isinstance(build_inputs, list), "build_inputs must be an array")
    build_input_files = {str(record["file"]) for record in build_inputs}
    require(
        build_input_files == EXPECTED_BUILD_INPUT_FILES,
        "required build input inventory differs",
    )

    contract = manifest["contract_headers"]
    require(isinstance(contract, dict), "contract_headers must be an object")
    contract_files = {str(record["file"]) for record in contract["files"]}
    require(
        contract_files == EXPECTED_CONTRACT_HEADER_FILES,
        "vendored contract header inventory differs",
    )

    license_records = manifest["license_files"]
    require(isinstance(license_records, list), "license_files must be an array")
    records_by_file = {str(record["file"]): record for record in license_records}
    require(
        set(records_by_file) == set(EXPECTED_LICENSE_RECORDS),
        "required license file inventory differs",
    )
    for filename, expected_fields in EXPECTED_LICENSE_RECORDS.items():
        record = records_by_file[filename]
        for key, expected in expected_fields.items():
            require(
                record.get(key) == expected,
                f"license provenance differs for {filename}: {key}",
            )

    notice = manifest["notice"]
    require(isinstance(notice, dict), "notice record must be an object")
    require(notice.get("file") == "NOTICE", "required NOTICE record differs")

    inventory = manifest["license_inventory"]
    require(isinstance(inventory, dict), "license_inventory must be an object")
    require(
        set(inventory) == set(EXPECTED_LICENSE_INVENTORY),
        "required license inventory categories differ",
    )
    listed_sources: set[str] = set()
    for category, expected_counts in EXPECTED_LICENSE_INVENTORY.items():
        item = inventory[category]
        actual_counts = (
            int(item["members"]),
            int(item["non_empty"]),
            int(item["empty"]),
        )
        require(
            actual_counts == expected_counts,
            f"license inventory counts differ for {category}",
        )
        source_files = item.get("source_files", [])
        require(
            isinstance(source_files, list),
            f"{category} source_files must be an array",
        )
        if source_files:
            require(
                len(source_files) == int(item["members"]),
                f"source file count differs for {category}",
            )
            overlap = listed_sources & set(source_files)
            require(not overlap, f"license source files overlap: {sorted(overlap)}")
            listed_sources.update(source_files)


def verify_archive(manifest: dict[str, object]) -> None:
    base = MANIFEST_PATH.parent
    verify_manifest_contract(manifest)
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

    non_empty_count, empty_count = archive_payload_counts(artifact)
    require(
        non_empty_count == int(artifact_record["non_empty_member_count"]),
        f"non-empty member count differs: {non_empty_count}",
    )
    require(
        empty_count == int(artifact_record["empty_member_count"]),
        f"empty member count differs: {empty_count}",
    )

    elf_headers = run_output(["readelf", "-h", str(artifact)])
    require(
        elf_headers.count("ELF Header:") == len(members),
        "not every archive member has an ELF header",
    )
    for marker in (
        "Class:                             ELF32",
        "Data:                              2's complement, little endian",
        "Machine:                           Tensilica Xtensa Processor",
        "Flags:                             0x300",
    ):
        require(
            elf_headers.count(marker) == len(members),
            f"archive member architecture differs: {marker.strip()}",
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
    contract_records = manifest["contract_headers"]["files"]
    for record in contract_records:
        verify_recorded_file(base, record, "contract header")
    recorded_headers = {
        resolve_recorded_path(base, str(record["file"]))
        for record in contract_records
    }
    actual_headers = {
        path.resolve()
        for path in (base / "include").iterdir()
        if path.is_file()
    }
    require(
        actual_headers == recorded_headers,
        "include/ directory and manifest contract headers differ",
    )
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

    source_inventory = manifest["source_inventory"]
    require(
        source_inventory["source_and_archive_member_count"] == len(members),
        "source inventory member count differs",
    )
    require(
        source_inventory["source_order_matches_archive_member_order"] is True,
        "source and archive member order is not recorded as matching",
    )
    require(
        source_inventory["members_with_code_or_data"] == non_empty_count,
        "source inventory non-empty count differs",
    )
    require(
        source_inventory["empty_members"] == empty_count,
        "source inventory empty count differs",
    )

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
        and name not in PLAIN_BLUEDROID_EXCEPTIONS
    )
    require(
        not plain,
        "final Classic ELF contains plain Bluedroid host symbols: "
        + ", ".join(plain[:20]),
    )
    leaked_internal = sorted(
        linked_defined
        & set(manifest["known_unprefixed_external_bluedroid_symbols"])
    )
    require(
        not leaked_internal,
        "final Classic ELF resolved dead Bluedroid internals from the core: "
        + ", ".join(leaked_internal[:20]),
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
