import re

MEASUREMENT_PATTERN = re.compile(
    rb"CP_MEASUREMENT valid=(\d+) flags=([0-9a-f]{4}) power=(-?\d+) context=(\w+)"
)


def test_cycling_power_service(dut, peers):
    """The Cycling Power server notifies a Measurement with 16-bit flags and a
    signed 16-bit instantaneous power; the client reads Sensor Location,
    subscribes, and decodes a negative power exactly."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("LOCATION_READ_REQUESTED", timeout=20)

    match = dut.expect(re.compile(rb"LOCATION_READ valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert match.group(1) == b"1", "Sensor Location read failed"
    assert int(match.group(2)) == 6, "expected Sensor Location Right Crank (6)"
    assert match.group(3) == b"loop"

    dut.expect_exact("CP_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("CP_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"CP_SUBSCRIPTION notifications=1"), timeout=20)

    device.write("h")
    device.expect_exact("CP_UPDATED stored=1 notified=1", timeout=10)
    measurement = dut.expect(MEASUREMENT_PATTERN, timeout=20)
    assert measurement.group(1) == b"1", "measurement too short"
    assert measurement.group(2) == b"0000", "expected 16-bit flags 0x0000"
    assert int(measurement.group(3)) == -30, "signed instantaneous power did not decode to -30"
    assert measurement.group(4) == b"loop", "must be delivered from update()/loop"

    device.expect(re.compile(rb"CP_SENT success=1"), timeout=20)

    dut.write("u")
    dut.expect_exact("CP_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("CP_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
