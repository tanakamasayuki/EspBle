import re


def test_alert_notification_service(dut, peers):
    """The Alert Notification Service server exposes a readable Supported New
    Alert Category bitmask, a notifiable New Alert, and a writable Alert
    Notification Control Point. The client reads the category bitmask (0x0022:
    Email + SMS/MMS), subscribes to New Alert, and writes "Notify New Alert
    Immediately" for the Email category; the server notifies a New Alert
    (category 1, count 3, text "Bob") which the client decodes."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("CATEGORY_READ_REQUESTED", timeout=20)

    category = dut.expect(re.compile(rb"CATEGORY_READ valid=(\d+) mask=([0-9a-f]{4}) context=(\w+)"), timeout=20)
    assert category.group(1) == b"1", "Supported New Alert Category read failed"
    assert category.group(2) == b"0022", "expected Email + SMS/MMS bitmask"
    assert category.group(3) == b"loop"

    dut.expect_exact("ALERT_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("ALERT_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"ALERT_SUBSCRIPTION notifications=1"), timeout=20)

    # Write the control point: Notify New Alert Immediately for Email (category 1).
    dut.write("c")
    dut.expect_exact("CONTROL_WRITE_REQUESTED", timeout=10)
    control = device.expect(
        re.compile(rb"CONTROL_WRITE command=(\d+) category=(\d+) context=(\w+)"), timeout=20)
    assert int(control.group(1)) == 2, "server should receive command 2"
    assert int(control.group(2)) == 1, "server should receive Email category 1"
    assert control.group(3) == b"loop"
    dut.expect_exact("CONTROL_WRITTEN success=1", timeout=20)

    alert = dut.expect(
        re.compile(rb"NEW_ALERT valid=(\d+) category=(\d+) count=(\d+) text=(\w+) context=(\w+)"), timeout=20)
    assert alert.group(1) == b"1", "New Alert too short"
    assert int(alert.group(2)) == 1, "New Alert category should be Email (1)"
    assert int(alert.group(3)) == 3, "New Alert count should be 3"
    assert alert.group(4) == b"Bob", "New Alert text should be 'Bob'"
    assert alert.group(5) == b"loop", "must be delivered from update()/loop"

    device.expect(re.compile(rb"ALERT_SENT success=1"), timeout=20)

    dut.write("u")
    dut.expect_exact("ALERT_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("ALERT_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
