def expect_secure_connection(dut, peripheral, connection_id, initial_value):
    dut.expect_exact(
        f"CENTRAL_CONNECTED id={connection_id} encrypted=0 bonded=0",
        timeout=20,
    )
    dut.expect_exact(
        "CENTRAL_SECURITY success=1 encrypted=1 authenticated=0 bonded=1 key=16 stored=1 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "PERIPHERAL_SECURITY success=1 encrypted=1 authenticated=0 bonded=1 key=16 stored=1 context=loop",
        timeout=20,
    )
    dut.expect_exact("DISCOVER_REQUESTED", timeout=10)
    dut.expect_exact("DISCOVER success=1 context=loop", timeout=20)
    dut.expect_exact("READ_REQUESTED", timeout=10)
    dut.expect_exact(
        f"READ success=1 value={initial_value} context=loop",
        timeout=20,
    )
    dut.expect_exact("WRITE_REQUESTED", timeout=10)
    dut.expect_exact("WRITE success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "SECURE_WRITE value=central-secure-write encrypted=1 bonded=1 context=loop",
        timeout=20,
    )


def test_security_bond_and_reconnect(dut, peers):
    peripheral = peers["device"]

    dut.write("x")
    peripheral.write("x")
    dut.expect_exact("CENTRAL_BONDS_CLEARED success=1 count=0", timeout=10)
    peripheral.expect_exact("PERIPHERAL_BONDS_CLEARED success=1 count=0", timeout=10)

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    expect_secure_connection(dut, peripheral, 1, "secure-ready")

    dut.write("b")
    peripheral.write("b")
    dut.expect_exact("CENTRAL_BONDS count=1", timeout=10)
    peripheral.expect_exact("PERIPHERAL_BONDS count=1", timeout=10)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("CENTRAL_DISCONNECTED id=1 context=loop", timeout=20)
    peripheral.expect_exact("PERIPHERAL_DISCONNECTED id=1 context=loop", timeout=20)

    peripheral.write("a")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    expect_secure_connection(dut, peripheral, 2, "central-secure-write")

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("CENTRAL_DISCONNECTED id=2 context=loop", timeout=20)
    peripheral.expect_exact("PERIPHERAL_DISCONNECTED id=2 context=loop", timeout=20)

    dut.write("x")
    peripheral.write("x")
    dut.expect_exact("CENTRAL_BONDS_CLEARED success=1 count=0", timeout=10)
    peripheral.expect_exact("PERIPHERAL_BONDS_CLEARED success=1 count=0", timeout=10)
