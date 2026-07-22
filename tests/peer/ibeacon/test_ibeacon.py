def test_ibeacon_broadcast_and_decode(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    # major 0x1234 = 4660, minor 0xABCD = 43981, measured power -59; a
    # non-connectable, non-scannable iBeacon.
    dut.expect_exact(
        "IBEACON uuid=0102030405060708090a0b0c0d0e0f10 "
        "major=4660 minor=43981 power=-59 connectable=0 scannable=0",
        timeout=20,
    )
