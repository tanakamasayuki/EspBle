def test_heart_rate_service(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("LOCATION_READ_REQUESTED", timeout=20)
    dut.expect_exact("LOCATION_READ valid=1 value=1 context=loop", timeout=20)
    dut.expect_exact("HEART_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("HEART_SUBSCRIBED success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "HEART_SUBSCRIPTION notifications=1 context=loop", timeout=20
    )

    peripheral.write("h")
    peripheral.expect_exact("HEART_UPDATED stored=1 notified=1", timeout=10)
    dut.expect_exact(
        "HEART_MEASUREMENT valid=1 flags=19 bpm=300 energy_present=1 energy=42 rr_count=1 first_rr=1024 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "HEART_SENT success=1 length=7 flags=19 context=loop", timeout=20
    )

    dut.write("u")
    dut.expect_exact("HEART_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("HEART_UNSUBSCRIBED success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "HEART_SUBSCRIPTION notifications=0 context=loop", timeout=20
    )
    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("DISCONNECTED id=1 context=loop", timeout=20)
