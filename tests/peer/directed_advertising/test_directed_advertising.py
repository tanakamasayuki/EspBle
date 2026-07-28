def test_directed_advertising_and_channel_map(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    # Undirected first: this is how both sides learn each other's address.
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("CENTRAL_CONNECTED id=1", timeout=20)
    peripheral.expect_exact("PERIPHERAL_CONNECTED id=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("CENTRAL_DISCONNECTED id=1", timeout=20)
    peripheral.expect_exact("PERIPHERAL_DISCONNECTED id=1", timeout=20)

    # Directed at that central. The advertisement carries no payload, so the
    # central connects by address instead of by scanning for the service.
    peripheral.write("D")
    peripheral.expect_exact("DIRECTED_TARGET success=1", timeout=10)
    peripheral.expect_exact("ADVERTISING 1", timeout=10)
    dut.write("c")
    dut.expect_exact("CONNECT_REQUESTED", timeout=10)
    dut.expect_exact("CENTRAL_CONNECTED id=2", timeout=20)
    peripheral.expect_exact("PERIPHERAL_CONNECTED id=2", timeout=20)

    dut.write("d")
    dut.expect_exact("CENTRAL_DISCONNECTED id=2", timeout=20)
    peripheral.expect_exact("PERIPHERAL_DISCONNECTED id=2", timeout=20)

    # Back to undirected, restricted to channel 39: still discoverable.
    peripheral.write("m")
    peripheral.expect_exact("CHANNEL_MAP success=1", timeout=10)
    peripheral.expect_exact("ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CENTRAL_CONNECTED id=3", timeout=30)
    peripheral.expect_exact("PERIPHERAL_CONNECTED id=3", timeout=20)

    dut.write("d")
    dut.expect_exact("CENTRAL_DISCONNECTED id=3", timeout=20)
    peripheral.expect_exact("PERIPHERAL_DISCONNECTED id=3", timeout=20)
    peripheral.write("u")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)
