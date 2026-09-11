import re

STATE_PATTERN = re.compile(rb"STATE valid=(\d+) current=(\d+) result=(\d+) context=(\w+)")


def test_reference_time_update_service(dut, peers):
    """The Reference Time Update Service server exposes a Write Without Response
    Time Update Control Point and a readable 2-byte Time Update State. The client
    reads the initial Idle state (0, 0), requests a reference update (Control
    Point 1 -> state Update Pending 1, 0), then cancels it (Control Point 2 ->
    state Idle with Canceled result 0, 1), re-reading the state each time."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("STATE_READ_REQUESTED", timeout=20)

    initial = dut.expect(STATE_PATTERN, timeout=20)
    assert initial.group(1) == b"1", "Time Update State read failed"
    assert int(initial.group(2)) == 0, "initial Current State should be Idle (0)"
    assert int(initial.group(3)) == 0, "initial Result should be Successful (0)"
    assert initial.group(4) == b"loop"

    # Get Reference Update -> Update Pending.
    dut.write("g")
    dut.expect_exact("GET_REQUESTED", timeout=10)
    get_cmd = device.expect(re.compile(rb"CONTROL_WRITE command=(\d+) context=(\w+)"), timeout=20)
    assert int(get_cmd.group(1)) == 1, "server should receive Get Reference Update (1)"
    assert get_cmd.group(2) == b"loop"

    dut.write("r")
    dut.expect_exact("STATE_READ_REQUESTED", timeout=10)
    pending = dut.expect(STATE_PATTERN, timeout=20)
    assert int(pending.group(2)) == 1, "Current State should be Update Pending (1)"

    # Cancel Reference Update -> Idle with Canceled result.
    dut.write("c")
    dut.expect_exact("CANCEL_REQUESTED", timeout=10)
    cancel_cmd = device.expect(re.compile(rb"CONTROL_WRITE command=(\d+) context=(\w+)"), timeout=20)
    assert int(cancel_cmd.group(1)) == 2, "server should receive Cancel Reference Update (2)"

    dut.write("r")
    dut.expect_exact("STATE_READ_REQUESTED", timeout=10)
    canceled = dut.expect(STATE_PATTERN, timeout=20)
    assert int(canceled.group(2)) == 0, "Current State should be back to Idle (0)"
    assert int(canceled.group(3)) == 1, "Result should be Canceled (1)"

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
