import re


def test_immediate_alert_service(dut, peers):
    """The Immediate Alert Service server exposes Alert Level (0x2A06) as a Write
    Without Response uint8. The client writes High Alert (2) then No Alert (0)
    without a response; the server observes each write in onWritten and reports
    the level from loop context."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect(re.compile(rb"CONNECTED id=(\d+)"), timeout=20)

    # High Alert via Write Without Response.
    dut.write("h")
    dut.expect_exact("HIGH_ALERT_REQUESTED", timeout=10)
    high = device.expect(re.compile(rb"ALERT_LEVEL level=(\d+) context=(\w+)"), timeout=20)
    assert int(high.group(1)) == 2, "server should receive High Alert level 2"
    assert high.group(2) == b"loop", "must be delivered from update()/loop"

    # No Alert via Write Without Response.
    dut.write("n")
    dut.expect_exact("NO_ALERT_REQUESTED", timeout=10)
    no_alert = device.expect(re.compile(rb"ALERT_LEVEL level=(\d+) context=(\w+)"), timeout=20)
    assert int(no_alert.group(1)) == 0, "server should receive No Alert level 0"
    assert no_alert.group(2) == b"loop"

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
