#!/usr/bin/env python3
"""Release gate for the checked-in Classic archive and its final link."""

from __future__ import annotations

import runpy
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VERIFY = ROOT / "tools" / "verify_classic_archive.py"
NAMESPACE_TEST = ROOT / "tests" / "unit" / "hci_router" / "test_classic_namespaced_calls.py"
NAMESPACE_TEST_NAMES = (
    "test_every_bluedroid_call_is_namespaced",
    "test_the_check_can_see_the_calls_it_guards",
)


def run(command: list[str]) -> None:
    subprocess.run(command, cwd=ROOT, check=True)


def one_output(build_dir: Path, suffix: str) -> Path:
    matches = list(build_dir.glob(f"*{suffix}"))
    if len(matches) != 1:
        raise RuntimeError(
            f"expected one {suffix} in {build_dir}, found {len(matches)}"
        )
    return matches[0]


def verify_namespaced_calls() -> None:
    namespace = runpy.run_path(str(NAMESPACE_TEST))
    for name in NAMESPACE_TEST_NAMES:
        test = namespace.get(name)
        if not callable(test):
            raise RuntimeError(f"required namespace test is missing: {name}")
        test()
    print("Classic source namespace guard OK")


def main() -> int:
    run([sys.executable, str(VERIFY)])
    verify_namespaced_calls()

    arduino_cli = shutil.which("arduino-cli")
    if not arduino_cli:
        raise RuntimeError("arduino-cli is required for the Classic release gate")

    with tempfile.TemporaryDirectory(prefix="espble-classic-release-gate-") as temporary:
        temporary_path = Path(temporary)
        classic_build = temporary_path / "esp32"
        other_build = temporary_path / "esp32s3"

        run(
            [
                arduino_cli,
                "compile",
                "--profile",
                "esp32",
                "--build-path",
                str(classic_build),
                "--clean",
                "examples/Classic/SppServer",
            ]
        )
        run(
            [
                arduino_cli,
                "compile",
                "--profile",
                "esp32s3",
                "--build-path",
                str(other_build),
                "--clean",
                "examples/CompileSmoke",
            ]
        )
        run(
            [
                sys.executable,
                str(VERIFY),
                "--classic-elf",
                str(one_output(classic_build, ".ino.elf")),
                "--classic-map",
                str(one_output(classic_build, ".ino.map")),
                "--other-map",
                str(one_output(other_build, ".ino.map")),
            ]
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
