import re

READ = re.compile(rb"READ handle=(\d+) value=(\S+)")
SUBSCRIBED = re.compile(rb"SUBSCRIBED handle=(\d+) ok=(\d)")
NOTIFY = re.compile(rb"NOTIFY handle=(\d+) value=(\S+)")
HANDLES = re.compile(rb"HANDLES first=(\d+) duplicate=(\d+) other=(\d+)")


def test_duplicate_uuid_registration(dut, peers):
    """Two services may share a UUID; two characteristics in one service may not.

    The bundled backend reuses an existing characteristic when a second one is
    added with the same UUID in the same service, so EspBle rejects that instead
    of returning a handle whose sends would go nowhere.
    """
    peripheral = peers["device"]

    peripheral.write("h")
    match = peripheral.expect(HANDLES, timeout=15)
    first, duplicate, other = (int(match.group(i)) for i in (1, 2, 3))
    assert first != 0, "the first characteristic should register"
    assert duplicate == 0, "a duplicate UUID in one service must be refused"
    assert other != 0, "the same UUID in a second service should register"
    assert first != other

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("c")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect("CENTRAL_CONNECTED id=", timeout=20)

    # Discovery runs straight through the NimBLE host API, so both instances of
    # the service UUID are enumerated -- the wrapper's UUID-keyed service map
    # would have dropped the second.
    dut.expect_exact("DISCOVERED services=2 characteristics=2", timeout=20)

    # Both characteristics are readable by attribute handle. The second one has
    # no object in the wrapper (its service repeats a UUID), so that read goes
    # through raw ATT.
    values, handles = [], []
    for _ in range(2):
        match = dut.expect(READ, timeout=15)
        handles.append(int(match.group(1)))
        values.append(match.group(2).decode())
    dut.expect_exact("READ_DONE", timeout=10)

    assert len(set(handles)) == 2, f"attribute handles are not distinct: {handles}"
    assert sorted(values) == ["first", "other"], f"unexpected values {values}"

    # Both are subscribable by handle. The second one's CCCD is written over raw
    # ATT, and its notifications arrive through the GAP event listener.
    for _ in range(2):
        match = dut.expect(SUBSCRIBED, timeout=15)
        assert match.group(2) == b"1", f"subscribe failed for handle {match.group(1)}"
    dut.expect_exact("SUBSCRIBE_DONE", timeout=10)

    # One notification at a time, so each arrival is attributable to a handle.
    for command, marker, payload in (("1", "first", "ping-first"), ("2", "other", "ping-other")):
        peripheral.write(command)
        peripheral.expect_exact(f"NOTIFIED {marker}=1", timeout=10)
        match = dut.expect(NOTIFY, timeout=15)
        assert int(match.group(1)) == handles[values.index(marker)], (
            f"{payload} arrived on handle {match.group(1)}"
        )
        assert match.group(2).decode() == payload
