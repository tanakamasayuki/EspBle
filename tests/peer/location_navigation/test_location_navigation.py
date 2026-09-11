import re

MEASUREMENT_PATTERN = re.compile(
    rb"LNS_MEASUREMENT valid=(\d+) flags=([0-9a-f]{4}) speed=(\d+) "
    rb"lat=(-?\d+) lon=(-?\d+) context=(\w+)"
)


def test_location_navigation_service(dut, peers):
    """The Location and Navigation server notifies a Location and Speed value
    carrying uint16 flags, Instantaneous Speed (1/100 m/s), and a sint32
    latitude/longitude (1e-7 deg); the client reads LN Feature and decodes
    5.00 m/s and the Tokyo coordinates exactly."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("FEATURE_READ_REQUESTED", timeout=20)

    match = dut.expect(re.compile(rb"FEATURE_READ valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert match.group(1) == b"1", "LN Feature read failed"
    assert int(match.group(2)) == 0x5, "expected Feature 0x00000005 (Speed + Location)"
    assert match.group(3) == b"loop"

    dut.expect_exact("LNS_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("LNS_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"LNS_SUBSCRIPTION notifications=1"), timeout=20)

    device.write("n")
    device.expect_exact("LNS_UPDATED stored=1 notified=1", timeout=10)
    measurement = dut.expect(MEASUREMENT_PATTERN, timeout=20)
    assert measurement.group(1) == b"1", "measurement too short"
    assert measurement.group(2) == b"0005", "expected Speed + Location present flags"
    assert int(measurement.group(3)) == 500, "5.00 m/s did not decode to 500"
    assert int(measurement.group(4)) == 356812000, "latitude 35.6812 deg did not decode"
    assert int(measurement.group(5)) == 1397671000, "longitude 139.7671 deg did not decode"
    assert measurement.group(6) == b"loop", "must be delivered from update()/loop"

    device.expect(re.compile(rb"LNS_SENT success=1"), timeout=20)

    dut.write("u")
    dut.expect_exact("LNS_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("LNS_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
