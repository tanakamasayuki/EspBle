import re

MEASUREMENT_PATTERN = re.compile(
    rb"WS_MEASUREMENT valid=(\d+) indication=(\d+) flags=([0-9a-f]{2}) grams=(-?\d+) context=(\w+)"
)


def test_weight_scale_service(dut, peers):
    """The Weight Scale server indicates a Measurement carrying a uint16 weight
    at 0.005 kg resolution; the client reads the Feature field and decodes
    70.000 kg (70000 g) exactly."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("FEATURE_READ_REQUESTED", timeout=20)

    match = dut.expect(re.compile(rb"FEATURE_READ valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert match.group(1) == b"1", "Weight Scale Feature read failed"
    assert int(match.group(2)) == 8, "expected Feature 0x00000008"
    assert match.group(3) == b"loop"

    dut.expect_exact("WS_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("WS_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"WS_SUBSCRIPTION indications=1"), timeout=20)

    device.write("h")
    device.expect_exact("WS_UPDATED stored=1 indicated=1", timeout=10)
    measurement = dut.expect(MEASUREMENT_PATTERN, timeout=20)
    assert measurement.group(1) == b"1", "measurement too short"
    assert measurement.group(2) == b"1", "must arrive as an indication"
    assert measurement.group(3) == b"00", "expected SI/no-optional flags"
    assert int(measurement.group(4)) == 70000, "70.000 kg did not decode to 70000 g"
    assert measurement.group(5) == b"loop", "must be delivered from update()/loop"

    device.expect(re.compile(rb"WS_SENT success=1"), timeout=20)

    dut.write("u")
    dut.expect_exact("WS_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("WS_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
