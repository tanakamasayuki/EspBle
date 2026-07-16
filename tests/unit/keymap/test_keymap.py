import subprocess
from pathlib import Path


def test_keymap_conversion():
    here = Path(__file__).parent
    source = here / "keymap_test.cpp"
    output = here / "output"
    output.mkdir(exist_ok=True)
    binary = output / "keymap_test"

    compile_result = subprocess.run(
        [
            "g++",
            "-std=c++17",
            # Match the Xtensa toolchain where plain char is unsigned.
            "-funsigned-char",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            str(here / ".." / ".." / ".." / "src"),
            str(source),
            "-o",
            str(binary),
        ],
        capture_output=True,
        text=True,
    )
    assert compile_result.returncode == 0, compile_result.stderr

    run_result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert run_result.returncode == 0, run_result.stdout + run_result.stderr
