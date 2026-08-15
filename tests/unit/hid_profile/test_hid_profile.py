import subprocess
from pathlib import Path


def test_hid_profile_descriptors():
    here = Path(__file__).parent
    root = here / ".." / ".." / ".."
    output = here / "output"
    output.mkdir(exist_ok=True)
    binary = output / "hid_profile_test"
    result = subprocess.run(
        [
            "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined",
            "-I", str(root / "src"),
            str(here / "hid_profile_test.cpp"),
            "-o", str(binary),
        ],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stderr
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
