import re

RESULT = re.compile(rb'RESULT mode=(\w+) name="([^"]*)" manufacturer=(\S+)')


def test_scan_response_fields_require_an_active_scan(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    # Passive scan: only the advertising payload arrives, which carries the
    # service UUID and nothing else.
    dut.write("p")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    match = dut.expect(RESULT, timeout=20)
    assert match.group(1) == b"passive"
    assert match.group(2) == b"", f"passive scan leaked the name: {match.group(2)!r}"
    assert match.group(3) == b"-", f"passive scan leaked manufacturer data: {match.group(3)!r}"

    # Active scan: the scan response is requested, so its name and manufacturer
    # data are merged into the same result.
    dut.write("a")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    match = dut.expect(RESULT, timeout=20)
    assert match.group(1) == b"active"
    assert match.group(2) == b"EspBle Scan Response", f"unexpected name {match.group(2)!r}"
    assert match.group(3) == b"ffff5152", f"unexpected manufacturer data {match.group(3)!r}"
