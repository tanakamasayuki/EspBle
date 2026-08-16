import re
import shutil
import tempfile
from pathlib import Path

import pexpect
import pytest

# `platform: esp32:esp32 (3.3.11)` in a sketch.yaml profile.
PLATFORM_PIN = re.compile(r"(platform:\s*esp32:esp32\s*\()[^)]+(\))")
BACKUP_DIR = Path(tempfile.gettempdir()) / "espble-core-version-backup"


def pytest_addoption(parser):
    group = parser.getgroup("espble core version")
    group.addoption(
        "--core-version",
        default=None,
        help="Build the DUT sketch against this arduino-esp32 version instead of the "
        "one pinned in sketch.yaml. Peers keep their pin, so the run measures the "
        "library itself on an older core.",
    )
    group.addoption(
        "--peer-core-version",
        default=None,
        help="Build the peer sketches against this arduino-esp32 version. The DUT "
        "keeps its pin, so the run measures interoperability with a peer built by an "
        "older core. Pass both options to move the whole suite to one version.",
    )


def pytest_report_header(config):
    lines = []
    if config.getoption("core_version"):
        lines.append(f"arduino-esp32 core (DUT): {config.getoption('core_version')}")
    if config.getoption("peer_core_version"):
        lines.append(f"arduino-esp32 core (peers only): {config.getoption('peer_core_version')}")
    return lines


def _sketch_yamls(session):
    """sketch.yaml files of the collected suites, split into DUT and peer.

    A suite's own sketch.yaml sits next to its test file; a peer sketch lives in
    a subdirectory with its own sketch.yaml. Collecting from the items rather
    than from the tree keeps the rewrite to the suites actually being run.
    """
    dut, peer = set(), set()
    for item in session.items:
        suite_dir = Path(item.fspath).parent
        own = suite_dir / "sketch.yaml"
        if own.exists():
            dut.add(own)
        for nested in suite_dir.glob("*/sketch.yaml"):
            peer.add(nested)
    return dut, peer


def pytest_collection_finish(session):
    """Repin the collected sketches before the plugin compiles anything.

    The pins stay committed at the supported version; this rewrite exists only
    while the run does. Doing it here rather than in a fixture guarantees it
    happens before any compile, whatever fixture order pytest picks.
    """
    config = session.config
    core_version = config.getoption("core_version")
    peer_core_version = config.getoption("peer_core_version")
    config._core_version_snapshots = {}
    if not core_version and not peer_core_version:
        return

    dut, peer = _sketch_yamls(session)
    targets = {}
    if core_version:
        targets.update({path: core_version for path in dut})
    if peer_core_version:
        targets.update({path: peer_core_version for path in peer})

    BACKUP_DIR.mkdir(parents=True, exist_ok=True)
    for path, version in sorted(targets.items()):
        original = path.read_text()
        config._core_version_snapshots[path] = original
        # A crash between here and sessionfinish leaves the working tree
        # rewritten, so the original also goes somewhere outside the repo.
        backup = BACKUP_DIR / (str(path.resolve()).strip("/").replace("/", "_"))
        backup.write_text(original)
        path.write_text(PLATFORM_PIN.sub(rf"\g<1>{version}\g<2>", original))
    print(
        f"\nRepinned {len(targets)} sketch.yaml file(s); originals also in {BACKUP_DIR}"
    )


def pytest_sessionfinish(session, exitstatus):
    for path, original in getattr(session.config, "_core_version_snapshots", {}).items():
        path.write_text(original)


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
