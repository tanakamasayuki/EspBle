import re
import time

QUERY_PATTERN = re.compile(
    rb"HOST_QUERY states=(\d+) releases=(\d+) connections=(\d+) ready=(\d+) invalid=(\d+)"
)
DISCOVERED_PATTERN = re.compile(
    rb"HOST_DISCOVERED success=(\d+) report=(\d+) output=(\d+) battery=(\d+)"
)


def _query(dut):
    dut.write("q")
    match = dut.expect(QUERY_PATTERN, timeout=10)
    return {
        "states": int(match.group(1)),
        "releases": int(match.group(2)),
        "connections": int(match.group(3)),
        "ready": int(match.group(4)),
        "invalid": int(match.group(5)),
    }


def test_boot_keyboard_without_report_ids(dut, peers):
    """A keyboard whose Report Map declares no report IDs (and therefore has
    no Report Reference descriptor) must be discovered and usable, and
    wrong-length input reports must be counted instead of vanishing."""
    device = peers["device"]

    dut.write("c")
    dut.expect_exact("HOST_COUNTERS_RESET", timeout=10)
    status = _query(dut)
    if status["connections"] != 0:
        dut.write("d")
        dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
        dut.expect(re.compile(rb"HOST_DISCONNECTED id=(\d+)"), timeout=20)
    device.write("?")
    device.expect_exact("DEVICE_ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("HOST_SCAN_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_CONNECT_STARTED success=1", timeout=20)
    dut.expect(re.compile(rb"HOST_CONNECTED id=(\d+)"), timeout=20)
    device.expect(re.compile(rb"DEVICE_CONNECTED id=(\d+)"), timeout=20)

    dut.write("i")
    dut.expect_exact("HOST_DISCOVERY_STARTED success=1", timeout=10)
    match = dut.expect(DISCOVERED_PATTERN, timeout=20)
    assert match.group(1) == b"1", "boot keyboard without report IDs must be discovered"
    assert match.group(2) == b"0", "report ID must be 0 when the map declares none"
    assert match.group(3) == b"0"
    assert match.group(4) == b"0"

    # A valid 8-byte report produces key state.
    device.write("k")
    device.expect_exact("DEVICE_NOTIFY_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_STATE a_down=1 a_released=0", timeout=20)
    before = _query(dut)
    assert before["ready"] == 1
    assert before["invalid"] == 0

    # A wrong-length report must not change key state but must be counted.
    device.write("w")
    device.expect_exact("DEVICE_NOTIFY_STARTED success=1", timeout=10)
    time.sleep(0.5)
    after = _query(dut)
    assert after["states"] == before["states"], "wrong-length report changed key state"
    assert after["invalid"] == before["invalid"] + 1, (
        "wrong-length input report was dropped without being counted"
    )

    device.write("r")
    device.expect_exact("DEVICE_NOTIFY_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_STATE a_down=0 a_released=1", timeout=20)

    dut.write("d")
    dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
    dut.expect(re.compile(rb"HOST_DISCONNECTED id=(\d+)"), timeout=20)
    device.expect_exact("DEVICE_READVERTISING 1", timeout=20)
