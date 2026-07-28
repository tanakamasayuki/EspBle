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
    # The sketch requests a 4 s connect timeout. The backend does not honour it
    # (it stays blocked for ~30 s), so EspBle abandons the attempt and reports
    # the failure itself. A 15 s budget passes only if that abandonment works.
    dut.expect("CENTRAL_CONNECT_FAILED", timeout=15)

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
