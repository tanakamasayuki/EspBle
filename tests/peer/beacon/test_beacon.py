import re


def test_beacon(dut, peers):
    """A non-connectable, non-scannable beacon (setConnectable(false) +
    setScanResponseEnabled(false)) broadcasts a marker service UUID and
    manufacturer data. The scanner captures it and confirms it is neither
    connectable nor scannable and carries the expected manufacturer payload
    (company 0xFFFF + 0x01020304)."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    beacon = dut.expect(re.compile(
        rb"BEACON connectable=(\d+) scannable=(\d+) mfglen=(\d+) mfg=([0-9a-f]*) context=(\w+)"),
        timeout=30)
    assert beacon.group(1) == b"0", "beacon must advertise as non-connectable"
    assert beacon.group(2) == b"0", "beacon must advertise as non-scannable"
    assert int(beacon.group(3)) == 6, "manufacturer data should be 6 bytes"
    assert beacon.group(4) == b"ffff01020304", "manufacturer payload must match"
    assert beacon.group(5) == b"loop", "scan result must be delivered from update()/loop"

    dut.write("x")
    dut.expect_exact("SCAN_STOPPED", timeout=10)
