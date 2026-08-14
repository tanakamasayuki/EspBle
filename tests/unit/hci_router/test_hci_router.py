import os
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


def test_hci_command_scheduler():
    here = Path(__file__).parent
    root = here / ".." / ".." / ".."
    output = here / "output"
    output.mkdir(exist_ok=True)
    binary = output / "hci_command_scheduler_test"
    result = subprocess.run(
        [
            "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-I", str(root / "src"),
            str(here / "hci_command_scheduler_test.cpp"),
            str(root / "src" / "EspBleHciCommandScheduler.c"),
            "-o", str(binary),
        ],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stderr
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_hci_fault_injection():
    """Randomized malformed input against all three modules, under sanitizers.

    The seed and iteration count are fixed so a failure is reproducible; raise
    ESPBLE_HCI_FUZZ_ITERATIONS for a longer soak when touching these modules.
    """
    here = Path(__file__).parent
    root = here / ".." / ".." / ".."
    output = here / "output"
    output.mkdir(exist_ok=True)
    binary = output / "hci_fuzz_test"
    result = subprocess.run(
        [
            "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer", "-g",
            "-I", str(root / "src"),
            str(here / "hci_fuzz_test.cpp"),
            str(root / "src" / "EspBleHciRouter.c"),
            str(root / "src" / "EspBleHciCommandScheduler.c"),
            str(root / "src" / "EspBleHciControllerPolicy.c"),
            "-o", str(binary),
        ],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stderr
    seed = os.environ.get("ESPBLE_HCI_FUZZ_SEED", "0x5eed1234")
    iterations = os.environ.get("ESPBLE_HCI_FUZZ_ITERATIONS", "200000")
    result = subprocess.run(
        [str(binary), seed, iterations],
        capture_output=True, text=True,
        env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"},
    )
    assert result.returncode == 0, result.stdout + result.stderr


def test_hci_controller_policy():
    here = Path(__file__).parent
    root = here / ".." / ".." / ".."
    output = here / "output"
    output.mkdir(exist_ok=True)
    binary = output / "hci_controller_policy_test"
    result = subprocess.run(
        [
            "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-I", str(root / "src"),
            str(here / "hci_controller_policy_test.cpp"),
            str(root / "src" / "EspBleHciControllerPolicy.c"),
            "-o", str(binary),
        ],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stderr
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
