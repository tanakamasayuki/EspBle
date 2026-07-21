import re

MEASUREMENT_PATTERN = re.compile(
    rb"BC_MEASUREMENT valid=(\d+) indication=(\d+) flags=([0-9a-f]{4}) "
    rb"fat_tenths=(-?\d+) weight_grams=(-?\d+) context=(\w+)"
)


def test_body_composition_service(dut, peers):
    """The Body Composition server indicates a Measurement carrying uint16 flags,
    the mandatory Body Fat Percentage (0.1 %/LSB), and the optional Weight field
    (0.005 kg/LSB in SI); the client reads the Feature field and decodes 27.5 %
    body fat and 70.000 kg (70000 g) exactly."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("FEATURE_READ_REQUESTED", timeout=20)

    match = dut.expect(re.compile(rb"FEATURE_READ valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert match.group(1) == b"1", "Body Composition Feature read failed"
    assert int(match.group(2)) == 0x200, "expected Feature 0x00000200 (Weight supported)"
    assert match.group(3) == b"loop"

    dut.expect_exact("BC_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("BC_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"BC_SUBSCRIPTION indications=1"), timeout=20)

    device.write("h")
    device.expect_exact("BC_UPDATED stored=1 indicated=1", timeout=10)
    measurement = dut.expect(MEASUREMENT_PATTERN, timeout=20)
    assert measurement.group(1) == b"1", "measurement too short"
    assert measurement.group(2) == b"1", "must arrive as an indication"
    assert measurement.group(3) == b"0400", "expected SI units with Weight-present flag"
    assert int(measurement.group(4)) == 275, "27.5 % body fat did not decode to 275 tenths"
    assert int(measurement.group(5)) == 70000, "70.000 kg did not decode to 70000 g"
    assert measurement.group(6) == b"loop", "must be delivered from update()/loop"

    device.expect(re.compile(rb"BC_SENT success=1"), timeout=20)

    dut.write("u")
    dut.expect_exact("BC_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("BC_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
