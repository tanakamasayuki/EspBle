def test_connect_disconnect(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("i")
    dut.expect_exact(
        "DIRECT_INVALID address=0/INVALID_ARGUMENT type=0/INVALID_ARGUMENT",
        timeout=10,
    )

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact(
        "CENTRAL_CONNECTED id=1 role=CENTRAL count=1 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "PERIPHERAL_CONNECTED id=1 role=PERIPHERAL count=1 context=loop",
        timeout=20,
    )

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact(
        "CENTRAL_DISCONNECTED id=1 role=CENTRAL count=0 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "PERIPHERAL_DISCONNECTED id=1 role=PERIPHERAL count=0 context=loop",
        timeout=20,
    )
    peripheral.expect_exact("READVERTISING 1", timeout=20)

    dut.write("r")
    dut.expect_exact("DIRECT_CONNECT_REQUESTED", timeout=10)
    dut.expect_exact(
        "CENTRAL_CONNECTED id=2 role=CENTRAL count=1 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "PERIPHERAL_CONNECTED id=2 role=PERIPHERAL count=1 context=loop",
        timeout=20,
    )

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact(
        "CENTRAL_DISCONNECTED id=2 role=CENTRAL count=0 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "PERIPHERAL_DISCONNECTED id=2 role=PERIPHERAL count=0 context=loop",
        timeout=20,
    )
