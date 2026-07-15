def test_advertise_scan(dut, peers):
    peripheral = peers["device"]

    # Query after both serial monitors are attached; do not depend on setup logs.
    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact(
        "SCAN_FOUND name=EspBle Peer service=1 manufacturer=3412beef context=loop",
        timeout=20,
    )
    dut.expect_exact("SCAN_STOPPED dropped=0", timeout=10)
