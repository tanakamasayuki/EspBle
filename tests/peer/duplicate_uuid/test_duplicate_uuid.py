import re

READ = re.compile(rb"READ handle=(\d+) value=(\S+)")
SUBSCRIBED = re.compile(rb"SUBSCRIBED handle=(\d+) ok=(\d)")
NOTIFY = re.compile(rb"NOTIFY handle=(\d+) value=(\S+)")
HANDLES = re.compile(rb"HANDLES first=(\d+) duplicate=(\d+) other=(\d+)")


def test_duplicate_uuid_registration(dut, peers):
    """Every duplication the spec allows works, in both roles.

    The local attribute table is built through the NimBLE host API, so two
    services may share a UUID and one service may hold two characteristics with
    the same UUID. On the client side discovery enumerates by handle, and read /
    write / subscribe all target a handle, so each instance is reachable.
    """
    peripheral = peers["device"]

    peripheral.write("h")
    match = peripheral.expect(HANDLES, timeout=15)
    first, duplicate, other = (int(match.group(i)) for i in (1, 2, 3))
    assert first != 0, "the first characteristic should register"
    assert duplicate != 0, "a duplicate UUID inside one service should register"
    assert other != 0, "the same UUID in a second service should register"
    assert len({first, duplicate, other}) == 3, "each registration needs its own handle"

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("c")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect("CENTRAL_CONNECTED id=", timeout=20)

    # Discovery runs straight through the NimBLE host API, so both instances of
    # the service UUID are enumerated -- the wrapper's UUID-keyed service map
    # would have dropped the second.
    dut.expect_exact("DISCOVERED services=2 characteristics=3", timeout=20)

    # Every instance is readable by attribute handle.
    values, handles = [], []
    for _ in range(3):
        match = dut.expect(READ, timeout=15)
        handles.append(int(match.group(1)))
        values.append(match.group(2).decode())
    dut.expect_exact("READ_DONE", timeout=10)

    assert len(set(handles)) == 3, f"attribute handles are not distinct: {handles}"
    assert sorted(values) == ["dup", "first", "other"], f"unexpected values {values}"

    # Each instance is subscribable by handle, and each CCCD is its own.
    for _ in range(3):
        match = dut.expect(SUBSCRIBED, timeout=15)
        assert match.group(2) == b"1", f"subscribe failed for handle {match.group(1)}"
    dut.expect_exact("SUBSCRIBE_DONE", timeout=10)

    # One notification at a time, so each arrival is attributable to a handle.
    for command, marker, payload in (
        ("1", "first", "ping-first"),
        ("2", "dup", "ping-dup"),
        ("3", "other", "ping-other"),
    ):
        peripheral.write(command)
        peripheral.expect_exact(f"NOTIFIED {marker}=1", timeout=10)
        match = dut.expect(NOTIFY, timeout=15)
        assert int(match.group(1)) == handles[values.index(marker)], (
            f"{payload} arrived on handle {match.group(1)}"
        )
        assert match.group(2).decode() == payload
