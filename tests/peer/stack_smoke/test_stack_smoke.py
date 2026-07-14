def test_bundled_stack_connect_read_write(dut, peers):
    peripheral = peers["device"]

    peripheral.expect_exact("PERIPHERAL_READY", timeout=20)
    dut.expect_exact("CENTRAL_READY", timeout=20)
    dut.expect_exact("SCAN_FOUND", timeout=30)
    dut.expect_exact("CONNECTED", timeout=20)
    peripheral.expect_exact("PEER_CONNECTED", timeout=20)
    dut.expect_exact("READ peer-ready", timeout=20)
    dut.expect_exact("WRITE 1", timeout=20)
    peripheral.expect_exact("WRITE central-write", timeout=20)
