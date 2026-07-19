def test_environmental_sensing_service(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("ENV_READ_REQUESTED", timeout=20)
    dut.expect_exact("ENV_READ index=0 valid=1 raw=-525 context=loop", timeout=20)
    dut.expect_exact("ENV_READ_REQUESTED", timeout=10)
    dut.expect_exact("ENV_READ index=1 valid=1 raw=4875 context=loop", timeout=20)
    dut.expect_exact("ENV_READ_REQUESTED", timeout=10)
    dut.expect_exact(
        "ENV_READ index=2 valid=1 raw=1013250 context=loop", timeout=20
    )
    dut.expect_exact("ENV_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("ENV_SUBSCRIBED success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "ENV_SUBSCRIPTION notifications=1 context=loop", timeout=20
    )

    peripheral.write("t")
    peripheral.expect_exact(
        "ENV_UPDATED stored=1 notified=1 raw=2345", timeout=10
    )
    dut.expect_exact(
        "ENV_TEMPERATURE_CHANGED valid=1 raw=2345 context=loop", timeout=20
    )
    peripheral.expect_exact("ENV_SENT success=1 length=2 context=loop", timeout=20)

    dut.write("u")
    dut.expect_exact("ENV_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("ENV_UNSUBSCRIBED success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "ENV_SUBSCRIPTION notifications=0 context=loop", timeout=20
    )
    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("DISCONNECTED id=1 context=loop", timeout=20)
