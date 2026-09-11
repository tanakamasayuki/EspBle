import re

MEASUREMENT_PATTERN = re.compile(
    rb"CSC_MEASUREMENT valid=(\d+) flags=([0-9a-f]{2}) wheel=(\d+) wheel_time=(\d+) "
    rb"crank=(\d+) crank_time=(\d+) context=(\w+)"
)


def test_cycling_speed_cadence_service(dut, peers):
    """The CSC server notifies a Measurement with cumulative wheel/crank
    revolutions and event times; the client reads Sensor Location, subscribes to
    notifications, and decodes every field of the multi-field layout."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("LOCATION_READ_REQUESTED", timeout=20)

    match = dut.expect(re.compile(rb"LOCATION_READ valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert match.group(1) == b"1", "Sensor Location read failed"
    assert int(match.group(2)) == 12, "expected Sensor Location Rear Hub (12)"
    assert match.group(3) == b"loop"

    dut.expect_exact("CSC_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("CSC_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"CSC_SUBSCRIPTION notifications=1"), timeout=20)

    device.write("h")
    device.expect_exact("CSC_UPDATED stored=1 notified=1", timeout=10)
    measurement = dut.expect(MEASUREMENT_PATTERN, timeout=20)
    assert measurement.group(1) == b"1", "measurement too short"
    assert measurement.group(2) == b"03", "expected wheel+crank flags"
    assert int(measurement.group(3)) == 100, "cumulative wheel revolutions mismatch"
    assert int(measurement.group(4)) == 2048, "last wheel event time mismatch"
    assert int(measurement.group(5)) == 50, "cumulative crank revolutions mismatch"
    assert int(measurement.group(6)) == 1024, "last crank event time mismatch"
    assert measurement.group(7) == b"loop", "must be delivered from update()/loop"

    device.expect(re.compile(rb"CSC_SENT success=1"), timeout=20)

    dut.write("u")
    dut.expect_exact("CSC_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("CSC_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
