def test_standalone_device_information(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("DIS_READ_REQUESTED", timeout=20)
    dut.expect_exact("DIS_TEXT index=0 value=EspBle context=loop", timeout=20)
    dut.expect_exact("DIS_READ_REQUESTED", timeout=10)
    dut.expect_exact("DIS_TEXT index=1 value=DIS-Peer context=loop", timeout=20)
    dut.expect_exact("DIS_READ_REQUESTED", timeout=10)
    dut.expect_exact("DIS_TEXT index=2 value=1.2.3 context=loop", timeout=20)
    dut.expect_exact("DIS_READ_REQUESTED", timeout=10)
    dut.expect_exact(
        "DIS_PNP valid=1 source=2 vendor=1234 product=5678 version=9abc context=loop",
        timeout=20,
    )
    dut.expect_exact("DIS_COMPLETE context=loop", timeout=10)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("DISCONNECTED id=1 context=loop", timeout=20)
