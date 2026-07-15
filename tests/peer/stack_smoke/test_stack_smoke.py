def test_bundled_stack_connect_read_write(dut, peers):
    peripheral = peers["device"]

    # The serial monitors can attach after the one-shot READY messages emitted
    # during setup. Synchronize on radio/GATT events, which are the behavior this
    # test is intended to verify and occur after both monitors are active.
    dut.expect_exact("SCAN_FOUND", timeout=30)
    dut.expect_exact("CONNECTED", timeout=20)
    peripheral.expect_exact("PEER_CONNECTED", timeout=20)
    dut.expect_exact("READ peer-ready", timeout=20)
    dut.expect_exact("WRITE 1", timeout=20)
    peripheral.expect_exact("WRITE central-write", timeout=20)
