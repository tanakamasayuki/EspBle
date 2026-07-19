def test_bundled_stack_connect_read_write(dut, peers):
    peripheral = peers["device"]

    # The serial monitors can attach after one-shot setup/connect messages.
    # Synchronize on the Central's connection and GATT results, then verify the
    # Peripheral's write callback. A successful read already proves that the
    # Peripheral accepted the connection.
    dut.expect_exact("SCAN_FOUND", timeout=30)
    dut.expect_exact("CONNECTED", timeout=20)
    dut.expect_exact("READ peer-ready", timeout=20)
    dut.expect_exact("WRITE 1", timeout=20)
    peripheral.expect_exact("WRITE central-write", timeout=20)
