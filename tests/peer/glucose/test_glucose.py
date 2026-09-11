import re
import time

GLUCOSE_PATTERN = re.compile(
    rb"GLUCOSE measurements=(\d+) seq=(\d+) concentration=(-?\d+) type_location=([0-9a-f]{2}) "
    rb"racp_responses=(\d+) racp0=(\d+) racp2=(\d+) racp3=(\d+) context=(\w+)"
)


def test_glucose_racp_procedure(dut, peers):
    """The Glucose RACP procedure: the client writes 'Report Stored Records
    (all)' to the Record Access Control Point; the server notifies one Glucose
    Measurement (sequence + base time + SFLOAT concentration) and then indicates
    the RACP response. Exercises the write -> notify -> indicate choreography."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect(re.compile(rb"FEATURE_READ valid=1"), timeout=20)
    dut.expect_exact("MEASUREMENT_SUBSCRIBED success=1", timeout=20)
    dut.expect_exact("RACP_SUBSCRIBED success=1", timeout=20)

    dut.write("r")
    dut.expect_exact("RACP_REQUESTED", timeout=10)
    device.expect(re.compile(rb"RACP_WRITE length=2 reportAll=1"), timeout=20)
    dut.expect_exact("RACP_WRITTEN success=1", timeout=20)
    device.expect(re.compile(rb"RACP_DONE success=1"), timeout=20)

    time.sleep(1.0)
    dut.write("q")
    match = dut.expect(GLUCOSE_PATTERN, timeout=10)
    assert int(match.group(1)) == 1, "expected exactly one measurement notification"
    assert int(match.group(2)) == 1, "sequence number mismatch"
    assert int(match.group(3)) == 99, "SFLOAT concentration mismatch"
    assert match.group(4) == b"11", "type/sample-location nibble byte mismatch"
    assert int(match.group(5)) == 1, "expected one RACP response indication"
    # RACP response: Response Code Op (0x06), request opcode (1), success (1).
    assert int(match.group(6)) == 6, "RACP response opcode mismatch"
    assert int(match.group(7)) == 1, "RACP request opcode mismatch"
    assert int(match.group(8)) == 1, "RACP response value (success) mismatch"
    assert match.group(9) == b"loop", "must be delivered from update()/loop"

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
