import re
import time

QUERY_PATTERN = re.compile(
    rb"HOST_QUERY connected=(\d+) disconnected=(\d+) states=(\d+) releases=(\d+) "
    rb"connections=(\d+) ready=(\d+) dropped=(\d+)"
)
DISCOVER_DISCONNECT_PATTERN = re.compile(
    rb"HOST_DISCOVER_DISCONNECT discover=(\d+) disconnect=(\d+) error=(\w+)"
)
REBEGIN_PATTERN = re.compile(rb"HOST_REBEGIN success=(\d+) error=(\w+)")
INPUT_SENT_PATTERN = re.compile(rb"DEVICE_INPUT_SENT success=(\d+) error=(\w+)")
READY_PATTERN = re.compile(rb"DEVICE_READY ready=(\d+) error_kept=(\d+)")


def _reset(dut, device):
    dut.write("c")
    dut.expect_exact("HOST_COUNTERS_RESET", timeout=10)
    dut.write("q")
    match = dut.expect(QUERY_PATTERN, timeout=10)
    if match.group(5) != b"0":
        dut.write("d")
        dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
        dut.expect(re.compile(rb"HOST_DISCONNECTED id=(\d+)"), timeout=20)
        dut.write("c")
        dut.expect_exact("HOST_COUNTERS_RESET", timeout=10)
    device.write("?")
    device.expect_exact("DEVICE_ADVERTISING 1", timeout=20)


def _connect(dut, device):
    dut.write("s")
    dut.expect_exact("HOST_SCAN_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_CONNECT_STARTED success=1", timeout=20)
    dut.expect(re.compile(rb"HOST_CONNECTED id=(\d+)"), timeout=20)
    device.expect(re.compile(rb"DEVICE_CONNECTED id=(\d+)"), timeout=20)


def _discover(dut):
    dut.write("i")
    dut.expect_exact("HOST_DISCOVERY_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_DISCOVERED success=1", timeout=20)


def _query(dut):
    dut.write("q")
    match = dut.expect(QUERY_PATTERN, timeout=10)
    return {
        "connected": int(match.group(1)),
        "disconnected": int(match.group(2)),
        "states": int(match.group(3)),
        "releases": int(match.group(4)),
        "connections": int(match.group(5)),
        "ready": int(match.group(6)),
        "dropped": int(match.group(7)),
    }


def test_unsubscribed_input_report_is_not_sent(dut, peers):
    """The device must not notify an input report to a Central that has not
    subscribed to it."""
    device = peers["device"]
    _reset(dut, device)
    _connect(dut, device)

    # No HID discovery has run, so nothing is subscribed yet.
    time.sleep(0.5)
    device.write("k")
    match = device.expect(INPUT_SENT_PATTERN, timeout=10)
    assert match.group(1) == b"0", "input report was sent without a subscriber"

    _discover(dut)
    device.write("k")
    match = device.expect(INPUT_SENT_PATTERN, timeout=10)
    assert match.group(1) == b"1"
    dut.expect_exact("HOST_STATE a_down=1 a_released=0", timeout=20)
    device.write("r")
    device.expect_exact("DEVICE_RELEASE_SENT success=1", timeout=10)
    dut.expect_exact("HOST_STATE a_down=0 a_released=1", timeout=20)

    dut.write("d")
    dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
    dut.expect(re.compile(rb"HOST_DISCONNECTED id=(\d+)"), timeout=20)
    device.expect_exact("DEVICE_READVERTISING 1", timeout=20)


def _device_ready(device):
    device.write("y")
    match = device.expect(READY_PATTERN, timeout=10)
    assert match.group(2) == b"1", "ready() must not touch lastError()"
    return match.group(1) == b"1"


def test_device_ready_follows_the_subscription_gate(dut, peers):
    """hidKeyboard().ready() must report exactly when a report can go out: false
    with no host, false while a connected host has not subscribed, true after
    the host's HID discovery subscribes, and false again after disconnection."""
    device = peers["device"]
    _reset(dut, device)

    assert not _device_ready(device), "ready() must be false with no host connected"

    _connect(dut, device)
    time.sleep(0.5)
    assert not _device_ready(device), (
        "ready() must be false while the connected host has not subscribed"
    )

    _discover(dut)
    assert _device_ready(device), "ready() must be true once the host subscribed"

    # The gate ready() reports is the one sendReport() applies.
    device.write("k")
    match = device.expect(INPUT_SENT_PATTERN, timeout=10)
    assert match.group(1) == b"1"
    dut.expect_exact("HOST_STATE a_down=1 a_released=0", timeout=20)
    device.write("r")
    device.expect_exact("DEVICE_RELEASE_SENT success=1", timeout=10)
    dut.expect_exact("HOST_STATE a_down=0 a_released=1", timeout=20)

    dut.write("d")
    dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
    dut.expect(re.compile(rb"HOST_DISCONNECTED id=(\d+)"), timeout=20)
    device.expect_exact("DEVICE_READVERTISING 1", timeout=20)
    assert not _device_ready(device), "ready() must be false again after disconnection"


def test_rollover_report_is_ignored(dut, peers):
    """A phantom/rollover report (six ErrorRollOver usages) must not be
    interpreted as releasing all held keys."""
    device = peers["device"]
    _reset(dut, device)
    _connect(dut, device)
    _discover(dut)

    device.write("k")
    device.expect(INPUT_SENT_PATTERN, timeout=10)
    dut.expect_exact("HOST_STATE a_down=1 a_released=0", timeout=20)
    before = _query(dut)

    device.write("o")
    device.expect_exact("DEVICE_ROLLOVER_SENT success=1", timeout=10)
    time.sleep(0.5)
    device.write("k")
    device.expect(INPUT_SENT_PATTERN, timeout=10)
    time.sleep(0.5)

    after = _query(dut)
    assert after["releases"] == before["releases"], (
        "rollover report was interpreted as a key release"
    )
    assert after["states"] == before["states"], (
        "rollover report produced state events"
    )

    device.write("r")
    device.expect_exact("DEVICE_RELEASE_SENT success=1", timeout=10)
    dut.expect_exact("HOST_STATE a_down=0 a_released=1", timeout=20)

    dut.write("d")
    dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
    dut.expect(re.compile(rb"HOST_DISCONNECTED id=(\d+)"), timeout=20)
    device.expect_exact("DEVICE_READVERTISING 1", timeout=20)


def test_release_event_survives_full_event_queue(dut, peers):
    """The all-release event synthesized on disconnection must survive a full
    HID event queue, so held keys never stay stuck."""
    device = peers["device"]
    _reset(dut, device)
    _connect(dut, device)
    _discover(dut)

    device.write("k")
    device.expect(INPUT_SENT_PATTERN, timeout=10)
    dut.expect_exact("HOST_STATE a_down=1 a_released=0", timeout=20)

    dut.write("p")
    dut.expect_exact("HOST_PAUSED", timeout=10)
    device.write("f")
    device.expect_exact("DEVICE_FLOOD_SENT sent=9", timeout=15)
    device.write("d")
    device.expect_exact("DEVICE_DISCONNECT_STARTED success=1", timeout=10)
    device.expect(re.compile(rb"DEVICE_DISCONNECTED id=(\d+)"), timeout=20)
    dut.expect_exact("HOST_RESUMED", timeout=20)

    status = _query(dut)
    assert status["disconnected"] == 1
    assert status["dropped"] >= 1, "queue overflow must be counted"
    assert status["releases"] >= 1, (
        "the all-release event was dropped by the full HID event queue "
        f"(status: {status})"
    )
    assert status["ready"] == 0

    device.expect_exact("DEVICE_READVERTISING 1", timeout=20)


def test_disconnect_deferred_during_discovery(dut, peers):
    """disconnect() on a connection whose HID discovery is still running is
    accepted but deferred: the worker runs to completion (the link is not pulled
    out from under it) and the disconnect then happens automatically, without a
    second disconnect() call."""
    device = peers["device"]
    _reset(dut, device)
    _connect(dut, device)

    dut.write("D")
    match = dut.expect(DISCOVER_DISCONNECT_PATTERN, timeout=20)
    assert match.group(1) == b"1", "discovery must start"
    assert match.group(2) == b"1", (
        "disconnect() during discovery must be accepted and deferred, not rejected "
        f"(error={match.group(3).decode()})"
    )
    assert match.group(3) == b"NONE", (
        f"a deferred disconnect must not report an error (error={match.group(3).decode()})"
    )
    # The worker finishes normally despite the pending disconnect...
    dut.expect_exact("HOST_DISCOVERED success=1", timeout=20)
    # ...then the deferred disconnect fires on its own (no second disconnect()).
    dut.expect(re.compile(rb"HOST_DISCONNECTED id=(\d+)"), timeout=20)
    device.expect_exact("DEVICE_READVERTISING 1", timeout=20)


def test_second_begin_with_different_config_fails(dut, peers):
    """A second begin() with a different configuration must fail instead of
    silently keeping the old configuration."""
    device = peers["device"]
    _reset(dut, device)

    dut.write("g")
    match = dut.expect(REBEGIN_PATTERN, timeout=10)
    assert match.group(1) == b"0", (
        "begin() with a different config silently reported success "
        f"(error={match.group(2).decode()})"
    )
