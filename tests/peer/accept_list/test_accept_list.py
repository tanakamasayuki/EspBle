def test_accept_list_blocks_and_admits_connections(dut, peers):
    peripheral = peers["device"]

    # Probe with a command rather than waiting for a boot banner: the sketch may
    # already have booted before the test attached to the serial port.
    # Only a bogus address is on the accept list, so under the restricted policy
    # the controller ignores every connection request.
    peripheral.write("r")
    peripheral.expect_exact("POLICY restricted entries=1", timeout=15)
    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("c")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    # 45 s, not the 4 s the sketch requests: when the peer's controller filters
    # the connection request away, EspBle's own timeout cancel does not cut the
    # attempt short and the failure only surfaces after the backend's internal
    # 30 s. Measured at ~31 s. See DESIGN_DEBT.ja.md "connect() timeoutが
    # filtered peerで効かない".
    dut.expect("CENTRAL_CONNECT_FAILED", timeout=45)

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
