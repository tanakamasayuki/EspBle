#!/usr/bin/env python3
"""Vendor the Bluedroid API headers the Classic archive was built against.

The archive in src/esp32/ bakes in the struct layouts of the ESP-IDF it was
built from. EspBle's sources used to take the matching declarations from
whatever Arduino-ESP32 core the sketch happened to compile against, which made
the compile-time contract drift with the core version while the archive stayed
fixed. This script pins the contract instead: it copies the public API headers
from the same IDF the archive came from into src/esp32/include/, where the
Classic sources include them by relative path and never read the core's copies.

The headers are copied byte-identically, so each file's provenance is provable
against the upstream tag by hash alone. Their internal includes are already in
quoted form, which resolves against the including file's directory first, so
placing the whole closure in one directory makes it self-contained; only
esp_err.h and the standard headers fall through to the core, by design.

Run this after (re)building the archive, pointing --source at the same IDF
checkout, so the pair can never go out of step:

  python3 tools/vendor_classic_contract.py \
      --source "$IDF_PATH/components/bt/host/bluedroid/api/include/api"

It rewrites src/esp32/include/ and the contract_headers section of
src/esp32/MANIFEST.json; verify_classic_archive.py checks both.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INCLUDE_DIR = ROOT / "src" / "esp32" / "include"
MANIFEST_PATH = ROOT / "src" / "esp32" / "MANIFEST.json"

# The include closure of every Bluedroid API header the Classic sources use.
# All fifteen live in components/bt/host/bluedroid/api/include/api/ upstream.
CONTRACT_HEADERS = [
    "esp_a2dp_api.h",
    "esp_a2dp_legacy_api.h",
    "esp_avrc_api.h",
    "esp_bluedroid_hci.h",
    "esp_bt_defs.h",
    "esp_bt_main.h",
    "esp_gap_bt_api.h",
    "esp_hf_ag_api.h",
    "esp_hf_ag_legacy_api.h",
    "esp_hf_client_api.h",
    "esp_hf_client_legacy_api.h",
    "esp_hf_defs.h",
    "esp_hidd_api.h",
    "esp_hidh_api.h",
    "esp_spp_api.h",
]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--source",
        required=True,
        help="directory holding the upstream API headers "
        "(…/components/bt/host/bluedroid/api/include/api of the pinned IDF)",
    )
    args = parser.parse_args()
    source = Path(args.source).expanduser().resolve()

    missing = [name for name in CONTRACT_HEADERS if not (source / name).is_file()]
    if missing:
        print(f"missing in {source}: {', '.join(missing)}", file=sys.stderr)
        return 2

    INCLUDE_DIR.mkdir(parents=True, exist_ok=True)
    stale = sorted(
        p.name
        for p in INCLUDE_DIR.glob("*.h")
        if p.name not in CONTRACT_HEADERS
    )
    for name in stale:
        (INCLUDE_DIR / name).unlink()
        print(f"removed stale header: {name}")

    records = []
    for name in CONTRACT_HEADERS:
        shutil.copyfile(source / name, INCLUDE_DIR / name)
        records.append({"file": f"include/{name}", "sha256": sha256(INCLUDE_DIR / name)})
        print(f"vendored {name}")

    manifest = json.loads(MANIFEST_PATH.read_text())
    manifest["contract_headers"] = {
        "description": "Bluedroid API headers the archive was built against, "
        "copied byte-identically from the pinned upstream so the compile-time "
        "contract cannot drift with the Arduino-ESP32 core version.",
        "upstream_directory": "components/bt/host/bluedroid/api/include/api",
        "files": records,
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"updated {MANIFEST_PATH.relative_to(ROOT)} ({len(records)} headers)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
