def test_gatt_read_write(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("DISCOVER_REQUESTED", timeout=20)
    dut.expect_exact(
        "DISCOVER success=1 read=1 write=1 context=loop",
        timeout=20,
    )
    dut.expect_exact("READ_REQUESTED", timeout=10)
    dut.expect_exact(
        "READ success=1 value=peer-ready context=loop",
        timeout=20,
    )
    dut.expect_exact("WRITE_REQUESTED", timeout=10)
    dut.expect_exact("WRITE success=1 context=loop", timeout=20)

    peripheral.expect_exact(
        "SERVER_WRITE id=1 value=central-write stored=1 context=loop",
        timeout=20,
    )
