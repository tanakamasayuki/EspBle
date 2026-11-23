def test_bundled_stack_connect_read_write(dut, peers, ready):
    peripheral = peers["device"]

    # The Central scans only once asked, so every line below is printed while
    # its serial reader is attached. A successful read already proves that the
    # Peripheral accepted the connection.
    ready(dut, "CENTRAL_READY")
    dut.write("g")
    dut.expect_exact("SCAN_FOUND", timeout=30)
    dut.expect_exact("CONNECTED", timeout=20)
    dut.expect_exact("READ peer-ready", timeout=20)
    dut.expect_exact("WRITE 1", timeout=20)
    peripheral.expect_exact("WRITE central-write", timeout=20)
