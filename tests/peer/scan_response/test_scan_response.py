import re

RESULT = re.compile(
    rb'RESULT mode=(\w+) name="([^"]*)" manufacturer=(\S+) appearance=(0x[0-9a-f]{4}) txpower=(\S+)'
)


def test_scan_response_fields_require_an_active_scan(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    # Passive scan: only the advertising payload arrives. It carries the service
    # UUID, the appearance and the Tx Power, but neither name nor manufacturer
    # data -- those live in the scan response.
    dut.write("p")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    match = dut.expect(RESULT, timeout=20)
    assert match.group(1) == b"passive"
    assert match.group(2) == b"", f"passive scan leaked the name: {match.group(2)!r}"
    assert match.group(3) == b"-", f"passive scan leaked manufacturer data: {match.group(3)!r}"
    assert match.group(4) == b"0x0341", f"unexpected appearance {match.group(4)!r}"
    passive_tx = match.group(5).decode()
    assert passive_tx != "-", "Tx Power Level missing from the advertising payload"
    # The controller writes the real transmit power, so only the range is fixed.
    assert -100 <= int(passive_tx) <= 20, f"implausible Tx Power {passive_tx}"

    # Active scan: the scan response is requested, so its name and manufacturer
    # data are merged into the same result. The advertising-payload fields stay.
    dut.write("a")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    match = dut.expect(RESULT, timeout=20)
    assert match.group(1) == b"active"
    assert match.group(2) == b"EspBle Scan Response", f"unexpected name {match.group(2)!r}"
    assert match.group(3) == b"ffff5152", f"unexpected manufacturer data {match.group(3)!r}"
    assert match.group(4) == b"0x0341", f"unexpected appearance {match.group(4)!r}"
    assert match.group(5).decode() == passive_tx, "Tx Power changed between scan modes"
