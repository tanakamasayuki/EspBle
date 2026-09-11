import re


def test_user_data_service(dut, peers):
    """The User Data Service server exposes a read/write Age, a writable First
    Name, and a read/write/notify Database Change Increment. Each write to Age or
    First Name is received by the server's onWritten, bumps the increment, and is
    notified. The client reads the initial Age (25), writes First Name and a new
    Age (42), sees the increment climb to 2, and re-reads Age as 42."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("DBCI_SUBSCRIBE_REQUESTED", timeout=20)
    dut.expect_exact("DBCI_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"DBCI_SUBSCRIPTION notifications=1"), timeout=20)

    # Initial Age read.
    dut.write("a")
    dut.expect_exact("AGE_READ_REQUESTED", timeout=10)
    age = dut.expect(re.compile(rb"AGE_READ valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert age.group(1) == b"1", "Age read failed"
    assert int(age.group(2)) == 25, "initial Age should be 25"
    assert age.group(3) == b"loop"

    # Write First Name -> server onWritten -> increment climbs to 1.
    dut.write("n")
    dut.expect_exact("NAME_WRITE_REQUESTED", timeout=10)
    name_write = device.expect(
        re.compile(rb"SERVER_WRITE char=name length=(\d+) first=(\w) context=(\w+)"), timeout=20)
    assert int(name_write.group(1)) == 3, "First Name should be 3 bytes"
    assert name_write.group(2) == b"A", "First Name should start with 'A'"
    assert name_write.group(3) == b"loop"
    dut.expect_exact("WRITTEN char=name success=1", timeout=20)
    notify1 = dut.expect(re.compile(rb"DBCI_NOTIFY valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert notify1.group(1) == b"1", "increment notification too short"
    assert int(notify1.group(2)) == 1, "increment should be 1 after first write"
    assert notify1.group(3) == b"loop"

    # Write a new Age -> server onWritten stores it, increment climbs to 2.
    dut.write("w")
    dut.expect_exact("AGE_WRITE_REQUESTED", timeout=10)
    age_write = device.expect(
        re.compile(rb"SERVER_WRITE char=age value=(\d+) context=(\w+)"), timeout=20)
    assert int(age_write.group(1)) == 42, "server should receive Age 42"
    assert age_write.group(2) == b"loop"
    dut.expect_exact("WRITTEN char=age success=1", timeout=20)
    notify2 = dut.expect(re.compile(rb"DBCI_NOTIFY valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert int(notify2.group(2)) == 2, "increment should be 2 after second write"

    # Re-read Age to confirm the server stored the written value.
    dut.write("a")
    dut.expect_exact("AGE_READ_REQUESTED", timeout=10)
    age2 = dut.expect(re.compile(rb"AGE_READ valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert int(age2.group(2)) == 42, "Age should read back as the written 42"

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
