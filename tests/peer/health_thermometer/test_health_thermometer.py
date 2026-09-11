import re

MEASUREMENT_PATTERN = re.compile(
    rb"THERM_MEASUREMENT valid=(\d+) indication=(\d+) flags=([0-9a-f]{2}) temp_x100=(-?\d+) context=(\w+)"
)


def test_health_thermometer_service(dut, peers):
    """The Health Thermometer server indicates a Temperature Measurement whose
    value is an IEEE-11073 32-bit FLOAT; the client reads Temperature Type,
    subscribes to indications, and decodes 37.5 C exactly."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("TYPE_READ_REQUESTED", timeout=20)

    # Temperature Type = 0x02 (Body).
    match = dut.expect(re.compile(rb"TYPE_READ valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert match.group(1) == b"1", "Temperature Type read failed"
    assert match.group(2) == b"2", "expected Temperature Type Body (2)"
    assert match.group(3) == b"loop"

    dut.expect_exact("THERM_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("THERM_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"THERM_SUBSCRIPTION indications=1"), timeout=20)

    # Trigger an indication and decode the FLOAT.
    device.write("h")
    device.expect_exact("THERM_UPDATED stored=1 indicated=1", timeout=10)
    measurement = dut.expect(MEASUREMENT_PATTERN, timeout=20)
    assert measurement.group(1) == b"1", "measurement too short"
    assert measurement.group(2) == b"1", "must arrive as an indication"
    assert measurement.group(3) == b"00", "expected Celsius/no-optional flags"
    assert int(measurement.group(4)) == 3750, "37.5 C did not decode exactly"
    assert measurement.group(5) == b"loop", "must be delivered from update()/loop"

    device.expect(re.compile(rb"THERM_SENT success=1"), timeout=20)

    dut.write("u")
    dut.expect_exact("THERM_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("THERM_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
