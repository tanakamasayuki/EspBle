import re
import time

QUERY_PATTERN = re.compile(
    rb"HOST_QUERY connected=(\d+) disconnected=(\d+) notifications=(\d+) "
    rb"connections=(\d+) ready=(\d+) dropped=(\d+) scan=(\d+)\r?\n"
)
END_CONNECT_PATTERN = re.compile(rb"HOST_END_CONNECT connect=(\d+) ms=(\d+) begin=(\d+)\r?\n")
CONNECT_FAILED_PATTERN = re.compile(rb"HOST_CONNECT_FAILED ms=(\d+) error=(\d+)\r?\n")
HEAP_PATTERN = re.compile(rb"HOST_HEAP free=(\d+)\r?\n")
END_CYCLE_PATTERN = re.compile(rb"HOST_END_CYCLE read=(\d+) begin=(\d+) heap=(\d+)\r?\n")
CONNECTED_PATTERN = re.compile(rb"HOST_CONNECTED id=(\d+)\r?\n")
DISCONNECTED_PATTERN = re.compile(rb"HOST_DISCONNECTED id=(\d+)\r?\n")


def _reset(dut, device):
    """Bring both boards to a known idle state without relying on boot banners."""
    dut.write("c")
    dut.expect_exact("HOST_COUNTERS_RESET", timeout=10)
    dut.write("q")
    match = dut.expect(QUERY_PATTERN, timeout=10)
    if match.group(4) != b"0":
        dut.write("d")
        dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
        dut.expect(DISCONNECTED_PATTERN, timeout=20)
        dut.write("c")
        dut.expect_exact("HOST_COUNTERS_RESET", timeout=10)
    device.write("?")
    device.expect_exact("DEVICE_ADVERTISING 1", timeout=20)


def _connect(dut, device):
    dut.write("s")
    dut.expect_exact("HOST_SCAN_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_CONNECT_STARTED success=1", timeout=20)
    dut.expect(CONNECTED_PATTERN, timeout=20)
    device.expect(re.compile(rb"DEVICE_CONNECTED id=(\d+)\r?\n"), timeout=20)


def test_peer_loss_is_detected_via_supervision_timeout(dut, peers):
    """When the peer vanishes silently (radio killed without a Link Layer
    terminate), the central must deliver a Disconnected event via the
    supervision timeout and release the connection slot.

    This must stay the LAST test in this module: the peer's BLE stack is
    unusable afterwards until the next module reflashes the board."""
    device = peers["device"]
    _reset(dut, device)

    _connect(dut, device)

    device.write("X")
    device.expect_exact("DEVICE_RADIO_KILLED", timeout=10)

    # NimBLE's default supervision timeout is a few seconds; allow margin.
    dut.expect(DISCONNECTED_PATTERN, timeout=30)

    dut.write("q")
    match = dut.expect(QUERY_PATTERN, timeout=10)
    assert int(match.group(2)) == 1, "Disconnected event must be delivered"
    assert int(match.group(4)) == 0, "backend connection slot must be released"
    assert int(match.group(5)) == 0, "HID Host must not report the lost peer as ready"
