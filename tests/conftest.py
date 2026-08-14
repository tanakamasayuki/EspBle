import shutil
from pathlib import Path

import pexpect
import pytest


def pytest_runtest_setup(item):
    output_dir = Path(item.fspath).parent / "output"
    if output_dir.exists():
        shutil.rmtree(output_dir)


@pytest.fixture
def probe():
    """Ask a board until it answers, instead of waiting for a startup banner.

    A line a sketch prints once at boot is gone if the serial monitor attaches
    after the reset, and a test that waits for it fails for a reason of its own
    making. Sketches answer the same line on a command as well, and this asks
    until the answer arrives.
    """

    def ask(target, command, pattern, attempts=12, timeout=5):
        for _ in range(attempts):
            target.write(command)
            try:
                return target.expect(pattern, timeout=timeout)
            except pexpect.TIMEOUT:
                continue
        raise AssertionError(f"no answer to {command!r} matching {pattern!r}")

    return ask
