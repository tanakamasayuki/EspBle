def test_accept_list_blocks_and_admits_connections(dut, peers):
    peripheral = peers["device"]

    # The peripheral boots with a restricted policy and only a bogus address on
    # the accept list, so the controller ignores every connection request.
    peripheral.expect_exact("POLICY restricted entries=1", timeout=15)
    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("c")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect("CENTRAL_CONNECT_FAILED", timeout=20)

    # Opening the policy lets the same central through.
    peripheral.write("o")
    peripheral.expect_exact("POLICY open entries=1", timeout=10)

    dut.write("c")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect("CENTRAL_CONNECTED id=", timeout=20)
    peripheral.expect("PERIPHERAL_CONNECTED id=", timeout=10)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect("CENTRAL_DISCONNECTED id=", timeout=15)
