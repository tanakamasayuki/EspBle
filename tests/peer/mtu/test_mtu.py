def test_mtu_negotiation_and_notification_limit(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    # The ATT MTU is exchanged just after the connection is up, so the
    # connected event still carries the 23-byte default on both roles; the
    # negotiated value arrives through onMtuChanged().
    dut.expect_exact("CENTRAL_CONNECTED id=1 mtu=23 payload=20", timeout=20)
    dut.expect_exact(
        "CENTRAL_MTU previous=23 mtu=128 stored=1 payload=125 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "PERIPHERAL_MTU previous=23 mtu=128 stored=1 payload=125 context=loop",
        timeout=20,
    )

    dut.write("n")
    dut.expect_exact("SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("SUBSCRIBED success=1 context=loop", timeout=20)
    peripheral.expect_exact("SUBSCRIPTION notifications=1 context=loop", timeout=20)

    peripheral.write("m")
    peripheral.expect_exact("MAX_NOTIFY_REQUESTED length=125", timeout=10)
    dut.expect_exact("NOTIFICATION length=125 indication=0 context=loop", timeout=20)
    peripheral.expect_exact("SENT success=1 length=125 context=loop", timeout=20)

    peripheral.write("o")
    peripheral.expect_exact("OVERSIZE_REJECTED error=INVALID_ARGUMENT", timeout=10)
