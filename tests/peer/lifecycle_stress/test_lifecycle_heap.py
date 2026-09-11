import re
import time

QUERY_PATTERN = re.compile(
    rb"HOST_QUERY connected=(\d+) disconnected=(\d+) notifications=(\d+) "
    rb"connections=(\d+) ready=(\d+) dropped=(\d+) scan=(\d+)"
)
END_CONNECT_PATTERN = re.compile(rb"HOST_END_CONNECT connect=(\d+) ms=(\d+) begin=(\d+)")
CONNECT_FAILED_PATTERN = re.compile(rb"HOST_CONNECT_FAILED ms=(\d+) error=(\d+)")
HEAP_PATTERN = re.compile(rb"HOST_HEAP free=(\d+)")
END_CYCLE_PATTERN = re.compile(rb"HOST_END_CYCLE read=(\d+) begin=(\d+) heap=(\d+)")
CONNECTED_PATTERN = re.compile(rb"HOST_CONNECTED id=(\d+)")
DISCONNECTED_PATTERN = re.compile(rb"HOST_DISCONNECTED id=(\d+)")


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
    device.expect(re.compile(rb"DEVICE_CONNECTED id=(\d+)"), timeout=20)


def test_reconnect_cycles_do_not_leak_heap(dut, peers):
    """Repeated connect + HID discovery + disconnect cycles must not leak
    the BLEClient and its remote service tree on every cycle."""
    device = peers["device"]
    _reset(dut, device)

    heaps = []
    cycles = 6
    for _ in range(cycles):
        _connect(dut, device)
        dut.write("i")
        dut.expect_exact("HOST_DISCOVERY_STARTED success=1", timeout=10)
        dut.expect_exact("HOST_DISCOVERED success=1", timeout=20)
        dut.write("d")
        dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
        dut.expect(DISCONNECTED_PATTERN, timeout=20)
        device.expect_exact("DEVICE_READVERTISING 1", timeout=20)
        dut.write("h")
        match = dut.expect(HEAP_PATTERN, timeout=10)
        heaps.append(int(match.group(1)))

    # Allow the first cycles to settle allocator pools; the remaining cycles
    # must not each leak a BLEClient plus its discovered service tree
    # (~10 KB/cycle before the fix, allocator noise stays well under 1 KB).
    settled = heaps[2:]
    loss = settled[0] - settled[-1]
    per_cycle = loss / (len(settled) - 1)
    assert loss < 4000, (
        f"heap shrank by {loss} bytes over {len(settled) - 1} cycles "
        f"(~{per_cycle:.0f} bytes/cycle, heap samples: {heaps})"
    )
