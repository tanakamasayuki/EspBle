def test_static_passkey_mitm(dut, peers):
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
    dut.expect_exact("CENTRAL_CONNECTED id=1", timeout=20)
    peripheral.expect_exact(
        "PASSKEY_DISPLAYED id=1 passkey=438209 context=loop",
        timeout=20,
    )
    dut.expect_exact(
        "CENTRAL_SECURITY success=1 encrypted=1 authenticated=1 bonded=1 key=16 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "PERIPHERAL_SECURITY success=1 encrypted=1 authenticated=1 bonded=1 key=16 context=loop",
        timeout=20,
    )
    dut.expect_exact("DISCOVER_REQUESTED", timeout=10)
    dut.expect_exact("DISCOVER success=1 context=loop", timeout=20)
    dut.expect_exact("READ_REQUESTED", timeout=10)
    dut.expect_exact(
        "READ success=1 value=authenticated-ready context=loop",
        timeout=20,
    )
    dut.expect_exact("WRITE_REQUESTED", timeout=10)
    dut.expect_exact("WRITE success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "AUTHENTICATED_WRITE value=authenticated-write authenticated=1 context=loop",
        timeout=20,
    )

    dut.write("b")
    peripheral.write("b")
    dut.expect_exact("CENTRAL_BONDS count=1", timeout=10)
    peripheral.expect_exact("PERIPHERAL_BONDS count=1", timeout=10)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("CENTRAL_DISCONNECTED id=1 context=loop", timeout=20)
    peripheral.expect_exact("PERIPHERAL_DISCONNECTED id=1 context=loop", timeout=20)

    dut.write("x")
    peripheral.write("x")
    dut.expect_exact("CENTRAL_BONDS_CLEARED success=1 count=0", timeout=10)
    peripheral.expect_exact("PERIPHERAL_BONDS_CLEARED success=1 count=0", timeout=10)
