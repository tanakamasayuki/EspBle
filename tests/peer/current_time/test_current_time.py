def test_current_time_service(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("TIME_READ_REQUESTED", timeout=20)
    dut.expect_exact(
        "TIME_READ valid=1 year=2026 month=7 day=19 hour=12 minute=34 second=56 weekday=7 fraction=128 reason=02 context=loop",
        timeout=20,
    )
    dut.expect_exact("TIME_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("TIME_SUBSCRIBED success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "TIME_SUBSCRIPTION notifications=1 context=loop", timeout=20
    )

    peripheral.write("t")
    peripheral.expect_exact(
        "TIME_UPDATED stored=1 notified=1 second=57", timeout=10
    )
    dut.expect_exact(
        "TIME_CHANGED valid=1 year=2026 month=7 day=19 hour=12 minute=34 second=57 weekday=7 fraction=128 reason=02 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "TIME_SENT success=1 length=10 second=57 context=loop", timeout=20
    )

    dut.write("u")
    dut.expect_exact("TIME_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("TIME_UNSUBSCRIBED success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "TIME_SUBSCRIPTION notifications=0 context=loop", timeout=20
    )
    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("DISCONNECTED id=1 context=loop", timeout=20)
