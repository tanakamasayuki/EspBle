import re


def test_proximity_profile(dut, peers):
    """The Proximity profile server hosts two services at once: the Link Loss
    Service (0x1803) with a read/write Alert Level, and the Tx Power Service
    (0x1804) with a read-only signed-int8 Tx Power Level. The client reads the
    Tx Power Level (-8 dBm), reads the initial Alert Level (0), writes High Alert
    (2) with response, and re-reads it as 2."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("TX_POWER_READ_REQUESTED", timeout=20)

    # Tx Power Level is a signed int8.
    tx = dut.expect(re.compile(rb"TX_POWER valid=(\d+) value=(-?\d+) context=(\w+)"), timeout=20)
    assert tx.group(1) == b"1", "Tx Power Level read failed"
    assert int(tx.group(2)) == -8, "Tx Power Level should decode to -8 dBm"
    assert tx.group(3) == b"loop"

    # Initial Link Loss Alert Level.
    dut.expect_exact("ALERT_READ_REQUESTED", timeout=10)
    initial = dut.expect(re.compile(rb"ALERT_LEVEL valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert initial.group(1) == b"1", "Alert Level read failed"
    assert int(initial.group(2)) == 0, "initial Alert Level should be No Alert (0)"

    # Write High Alert with response.
    dut.write("w")
    dut.expect_exact("ALERT_WRITE_REQUESTED", timeout=10)
    server_write = device.expect(re.compile(rb"ALERT_WRITE level=(\d+) context=(\w+)"), timeout=20)
    assert int(server_write.group(1)) == 2, "server should receive High Alert level 2"
    assert server_write.group(2) == b"loop"
    dut.expect_exact("ALERT_WRITTEN success=1", timeout=20)

    # Re-read Alert Level to confirm the server stored it.
    dut.write("a")
    dut.expect_exact("ALERT_READ_REQUESTED", timeout=10)
    after = dut.expect(re.compile(rb"ALERT_LEVEL valid=(\d+) value=(\d+) context=(\w+)"), timeout=20)
    assert int(after.group(2)) == 2, "Alert Level should read back as the written 2"

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
