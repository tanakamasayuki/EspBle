import re

MEASUREMENT_PATTERN = re.compile(
    rb"CGM_MEASUREMENT valid=(\d+) crc_ok=(\d+) size=(\d+) flags=([0-9a-f]{2}) "
    rb"glucose=(-?\d+) time_offset=(\d+) context=(\w+)"
)


def test_continuous_glucose_monitoring_service(dut, peers):
    """The CGM server exposes an E2E-CRC-protected CGM Feature and notifies a CGM
    Measurement whose SFLOAT glucose concentration, time offset, and appended
    E2E-CRC (CRC-16/MCRF4XX) the client verifies and decodes. The CRC is produced
    and checked by the shared EspBleCgmCrc.h codec on both boards."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("FEATURE_READ_REQUESTED", timeout=20)

    feature = dut.expect(re.compile(
        rb"FEATURE_READ valid=(\d+) crc_ok=(\d+) feature=([0-9a-f]{6}) "
        rb"type_location=([0-9a-f]{2}) context=(\w+)"), timeout=20)
    assert feature.group(1) == b"1", "CGM Feature read failed"
    assert feature.group(2) == b"1", "CGM Feature E2E-CRC did not verify"
    assert feature.group(3) == b"001000", "expected E2E-CRC-supported feature bit"
    assert feature.group(4) == b"11", "expected Type/Sample Location 0x11"
    assert feature.group(5) == b"loop"

    dut.expect_exact("CGM_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("CGM_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"CGM_SUBSCRIPTION notifications=1"), timeout=20)

    device.write("m")
    device.expect_exact("CGM_UPDATED stored=1 notified=1", timeout=10)
    measurement = dut.expect(MEASUREMENT_PATTERN, timeout=20)
    assert measurement.group(1) == b"1", "measurement wrong length"
    assert measurement.group(2) == b"1", "measurement E2E-CRC did not verify"
    assert int(measurement.group(3)) == 8, "Size field should be 8"
    assert measurement.group(4) == b"00", "expected no optional-field flags"
    assert int(measurement.group(5)) == 100, "glucose concentration should be 100"
    assert int(measurement.group(6)) == 5, "time offset should be 5 minutes"
    assert measurement.group(7) == b"loop", "must be delivered from update()/loop"

    device.expect(re.compile(rb"CGM_SENT success=1"), timeout=20)

    dut.write("u")
    dut.expect_exact("CGM_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("CGM_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
