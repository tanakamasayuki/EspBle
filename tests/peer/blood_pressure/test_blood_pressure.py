import re

MEASUREMENT_PATTERN = re.compile(
    rb"BP_MEASUREMENT valid=(\d+) indication=(\d+) flags=([0-9a-f]{2}) "
    rb"systolic=(-?\d+) diastolic=(-?\d+) mean=(-?\d+) context=(\w+)"
)


def test_blood_pressure_service(dut, peers):
    """The Blood Pressure server indicates a Measurement whose systolic /
    diastolic / mean values are IEEE-11073 16-bit SFLOATs; the client reads the
    Feature field and decodes 120/80/93 mmHg exactly."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("FEATURE_READ_REQUESTED", timeout=20)

    match = dut.expect(re.compile(rb"FEATURE_READ valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert match.group(1) == b"1", "Blood Pressure Feature read failed"
    assert int(match.group(2)) == 3, "expected Feature bits 0x0003"
    assert match.group(3) == b"loop"

    dut.expect_exact("BP_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("BP_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"BP_SUBSCRIPTION indications=1"), timeout=20)

    device.write("h")
    device.expect_exact("BP_UPDATED stored=1 indicated=1", timeout=10)
    measurement = dut.expect(MEASUREMENT_PATTERN, timeout=20)
    assert measurement.group(1) == b"1", "measurement too short"
    assert measurement.group(2) == b"1", "must arrive as an indication"
    assert measurement.group(3) == b"00", "expected mmHg/no-optional flags"
    assert int(measurement.group(4)) == 120, "systolic SFLOAT mismatch"
    assert int(measurement.group(5)) == 80, "diastolic SFLOAT mismatch"
    assert int(measurement.group(6)) == 93, "mean arterial pressure SFLOAT mismatch"
    assert measurement.group(7) == b"loop", "must be delivered from update()/loop"

    device.expect(re.compile(rb"BP_SENT success=1"), timeout=20)

    dut.write("u")
    dut.expect_exact("BP_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("BP_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
