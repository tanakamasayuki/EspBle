import re


def test_phone_alert_status_service(dut, peers):
    """The Phone Alert Status Service server exposes a read/notify Alert Status
    and Ringer Setting plus a Write Without Response Ringer Control Point. The
    client reads Alert Status (0x01), subscribes to Ringer Setting, reads the
    initial Normal setting (1), then drives the Control Point to Set Silent Mode
    (Ringer Setting notifies 0) and Cancel Silent Mode (notifies 1)."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("ALERT_STATUS_READ_REQUESTED", timeout=20)

    status = dut.expect(re.compile(rb"ALERT_STATUS_READ valid=(\d+) status=([0-9a-f]{2}) context=(\w+)"), timeout=20)
    assert status.group(1) == b"1", "Alert Status read failed"
    assert status.group(2) == b"01", "expected Ringer State active (0x01)"
    assert status.group(3) == b"loop"

    dut.expect_exact("RINGER_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("RINGER_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"RINGER_SUBSCRIPTION notifications=1"), timeout=20)

    # Initial Ringer Setting read.
    dut.write("r")
    dut.expect_exact("RINGER_READ_REQUESTED", timeout=10)
    initial = dut.expect(re.compile(rb"RINGER_SETTING valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert initial.group(1) == b"1", "Ringer Setting read failed"
    assert int(initial.group(2)) == 1, "initial Ringer Setting should be Normal (1)"

    # Set Silent Mode -> Ringer Setting notifies 0.
    dut.write("q")
    dut.expect_exact("SILENT_REQUESTED", timeout=10)
    silent_cmd = device.expect(re.compile(rb"CONTROL_WRITE command=(\d+) context=(\w+)"), timeout=20)
    assert int(silent_cmd.group(1)) == 1, "server should receive Set Silent Mode (1)"
    assert silent_cmd.group(2) == b"loop"
    silent = dut.expect(re.compile(rb"RINGER_NOTIFY valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert int(silent.group(2)) == 0, "Silent Mode should notify Ringer Setting 0"
    assert silent.group(3) == b"loop"

    # Cancel Silent Mode -> Ringer Setting notifies 1.
    dut.write("c")
    dut.expect_exact("CANCEL_REQUESTED", timeout=10)
    cancel_cmd = device.expect(re.compile(rb"CONTROL_WRITE command=(\d+) context=(\w+)"), timeout=20)
    assert int(cancel_cmd.group(1)) == 3, "server should receive Cancel Silent Mode (3)"
    normal = dut.expect(re.compile(rb"RINGER_NOTIFY valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert int(normal.group(2)) == 1, "Cancel Silent Mode should notify Ringer Setting 1"

    dut.write("u")
    dut.expect_exact("RINGER_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("RINGER_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
