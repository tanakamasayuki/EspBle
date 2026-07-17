import re
import time

QUERY_PATTERN = re.compile(
    rb"HOST_QUERY connected=(\d+) disconnected=(\d+) notifications=(\d+) "
    rb"connections=(\d+) ready=(\d+) dropped=(\d+) scan=(\d+)"
)
END_CONNECT_PATTERN = re.compile(rb"HOST_END_CONNECT connect=(\d+) ms=(\d+) begin=(\d+)")
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


def test_disconnect_event_survives_notification_flood(dut, peers):
    """A Disconnected event must not be silently dropped when the shared
    event queue is filled with notifications while update() is not being
    called, and the HID Host slot must be released."""
    device = peers["device"]
    _reset(dut, device)

    _connect(dut, device)

    dut.write("i")
    dut.expect_exact("HOST_DISCOVERY_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_DISCOVERED success=1", timeout=20)
    dut.write("v")
    dut.expect_exact("HOST_SUBSCRIBE_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_SUBSCRIBED success=1", timeout=20)

    dut.write("q")
    match = dut.expect(QUERY_PATTERN, timeout=10)
    assert match.group(5) == b"1", "HID Host must be ready after discovery"

    # Stop dispatching events on the host, then flood the shared event queue
    # with battery notifications and disconnect while it is full.
    dut.write("p")
    dut.expect_exact("HOST_PAUSED", timeout=10)
    device.write("B")
    device.expect_exact("DEVICE_BATTERY_BURST sent=10", timeout=10)
    device.write("d")
    device.expect_exact("DEVICE_DISCONNECT_STARTED success=1", timeout=10)
    device.expect(re.compile(rb"DEVICE_DISCONNECTED id=(\d+)"), timeout=20)
    dut.expect_exact("HOST_RESUMED", timeout=20)

    dut.write("q")
    match = dut.expect(QUERY_PATTERN, timeout=10)
    connected = int(match.group(1))
    disconnected = int(match.group(2))
    notifications = int(match.group(3))
    connections = int(match.group(4))
    ready = int(match.group(5))
    dropped = int(match.group(6))

    assert connected == 1
    assert connections == 0, "backend connection slot must be released"
    assert notifications >= 1, "at least some notifications must be delivered"
    # The lifecycle event must survive the notification flood.
    assert disconnected == 1, (
        "Disconnected event was dropped by the full event queue "
        f"(notifications delivered: {notifications})"
    )
    # The HID Host slot must be invalidated on disconnection.
    assert ready == 0, "HID Host still reports a stale connection as ready"
    # Queue overflow must be observable.
    assert dropped >= 1, "dropped notifications must be counted"

    # Writing LEDs to the stale/released slot must fail cleanly, not crash.
    dut.write("l")
    dut.expect_exact("HOST_LEDS success=0", timeout=20)


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


def test_end_cancels_pending_connect(dut, peers):
    """end() while a connect attempt to an unreachable peer is in flight must
    cancel it instead of blocking until the connect timeout (10s+)."""
    device = peers["device"]
    _reset(dut, device)

    dut.write("Z")
    match = dut.expect(END_CONNECT_PATTERN, timeout=40)
    assert match.group(1) == b"1", "connect() must be accepted"
    assert match.group(3) == b"1", "begin() must succeed after end()"
    duration = int(match.group(2))
    assert duration < 3000, (
        f"end() blocked for {duration} ms waiting for the connect timeout"
    )


def test_end_flushes_scanner_queue(dut, peers):
    """Scan results queued but never dispatched must not leak into the next
    begin() session."""
    device = peers["device"]
    _reset(dut, device)

    dut.write("Y")
    dut.expect_exact("HOST_SCAN_FLUSH begin=1", timeout=30)
    time.sleep(1.0)
    dut.write("q")
    match = dut.expect(QUERY_PATTERN, timeout=10)
    assert int(match.group(7)) == 0, (
        f"{int(match.group(7))} stale scan result(s) from the previous "
        "session were delivered after begin()"
    )
    dut.write("c")
    dut.expect_exact("HOST_COUNTERS_RESET", timeout=10)


def test_end_during_gatt_operation_stress(dut, peers):
    """end() while a GATT worker task is completing must not use freed
    state. Exercises the completion window repeatedly."""
    device = peers["device"]
    _reset(dut, device)

    for cycle in range(4):
        _connect(dut, device)
        dut.write("z")
        match = dut.expect(END_CYCLE_PATTERN, timeout=30)
        assert match.group(1) == b"1", f"GATT read did not start on cycle {cycle}"
        assert match.group(2) == b"1", f"begin() failed after end() on cycle {cycle}"
        device.expect_exact("DEVICE_READVERTISING 1", timeout=20)

    # The host must still be responsive after the stress cycles.
    dut.write("q")
    dut.expect(QUERY_PATTERN, timeout=10)
