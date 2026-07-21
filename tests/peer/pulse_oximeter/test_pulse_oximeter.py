import re

MEASUREMENT_PATTERN = re.compile(
    rb"PLX_MEASUREMENT valid=(\d+) indication=(\d+) flags=([0-9a-f]{2}) "
    rb"spo2=(-?\d+) pulse=(-?\d+) context=(\w+)"
)


def test_pulse_oximeter_service(dut, peers):
    """The Pulse Oximeter server indicates a Spot-Check Measurement whose SpO2
    and pulse rate are IEEE-11073 16-bit SFLOATs; the client reads PLX Features
    and decodes 98 % / 60 bpm exactly."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("FEATURES_READ_REQUESTED", timeout=20)

    match = dut.expect(re.compile(rb"FEATURES_READ valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert match.group(1) == b"1", "PLX Features read failed"
    assert int(match.group(2)) == 3, "expected PLX Features 0x0003"
    assert match.group(3) == b"loop"

    dut.expect_exact("PLX_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("PLX_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"PLX_SUBSCRIPTION indications=1"), timeout=20)

    device.write("h")
    device.expect_exact("PLX_UPDATED stored=1 indicated=1", timeout=10)
    measurement = dut.expect(MEASUREMENT_PATTERN, timeout=20)
    assert measurement.group(1) == b"1", "measurement too short"
    assert measurement.group(2) == b"1", "must arrive as an indication"
    assert measurement.group(3) == b"00", "expected no-optional flags"
    assert int(measurement.group(4)) == 98, "SpO2 SFLOAT mismatch"
    assert int(measurement.group(5)) == 60, "pulse rate SFLOAT mismatch"
    assert measurement.group(6) == b"loop", "must be delivered from update()/loop"

    device.expect(re.compile(rb"PLX_SENT success=1"), timeout=20)

    dut.write("u")
    dut.expect_exact("PLX_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("PLX_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
