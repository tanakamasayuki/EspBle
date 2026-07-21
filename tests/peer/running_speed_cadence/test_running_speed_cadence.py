import re

MEASUREMENT_PATTERN = re.compile(
    rb"RSC_MEASUREMENT valid=(\d+) flags=([0-9a-f]{2}) speed=(\d+) cadence=(\d+) "
    rb"stride=(\d+) distance=(\d+) context=(\w+)"
)


def test_running_speed_cadence_service(dut, peers):
    """The RSC server notifies a Measurement with a mixed-width layout (uint16
    speed, uint8 cadence, optional uint16 stride length and uint32 total
    distance); the client reads Sensor Location, subscribes, and decodes every
    present field."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("LOCATION_READ_REQUESTED", timeout=20)

    match = dut.expect(re.compile(rb"LOCATION_READ valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert match.group(1) == b"1", "Sensor Location read failed"
    assert int(match.group(2)) == 2, "expected Sensor Location In Shoe (2)"
    assert match.group(3) == b"loop"

    dut.expect_exact("RSC_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("RSC_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"RSC_SUBSCRIPTION notifications=1"), timeout=20)

    device.write("h")
    device.expect_exact("RSC_UPDATED stored=1 notified=1", timeout=10)
    measurement = dut.expect(MEASUREMENT_PATTERN, timeout=20)
    assert measurement.group(1) == b"1", "measurement too short"
    assert measurement.group(2) == b"03", "expected stride+distance flags"
    assert int(measurement.group(3)) == 768, "instantaneous speed mismatch"
    assert int(measurement.group(4)) == 180, "instantaneous cadence mismatch"
    assert int(measurement.group(5)) == 125, "stride length mismatch"
    assert int(measurement.group(6)) == 10000, "total distance mismatch"
    assert measurement.group(7) == b"loop", "must be delivered from update()/loop"

    device.expect(re.compile(rb"RSC_SENT success=1"), timeout=20)

    dut.write("u")
    dut.expect_exact("RSC_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("RSC_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
