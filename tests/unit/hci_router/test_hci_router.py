import subprocess
from pathlib import Path


def test_hci_router():
    here = Path(__file__).parent
    root = here / ".." / ".." / ".."
    output = here / "output"
    output.mkdir(exist_ok=True)
    binary = output / "hci_router_test"
    result = subprocess.run(
        [
            "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-I", str(root / "src"),
            str(here / "hci_router_test.cpp"),
            str(root / "src" / "EspBleHciRouter.c"),
            "-o", str(binary),
        ],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stderr
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
