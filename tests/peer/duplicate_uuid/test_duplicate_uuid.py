import re

READ = re.compile(rb"READ handle=(\d+) value=(\S+)")


def test_duplicate_uuids_are_addressable(dut, peers):
    peripheral = peers["device"]

    # The peer registers two services with one UUID and three characteristics
    # with another; the handles it got back must all differ.
    peripheral.write("h")
    peripheral.expect(re.compile(rb"HANDLES first=(\d+) second=(\d+) other=(\d+)"), timeout=15)
    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=15)

    dut.write("c")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect("CENTRAL_CONNECTED id=", timeout=20)

    # A UUID-keyed server could not have created these at all.
    dut.expect_exact("DISCOVERED services=2 characteristics=3", timeout=20)

    values, handles = [], []
    for _ in range(3):
        match = dut.expect(READ, timeout=15)
        handles.append(int(match.group(1)))
        values.append(match.group(2).decode())
    dut.expect_exact("READ_DONE", timeout=10)

    assert len(set(handles)) == 3, f"attribute handles are not distinct: {handles}"
    assert sorted(values) == ["first", "other", "second"], f"unexpected values {values}"
