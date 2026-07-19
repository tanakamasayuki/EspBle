def test_standalone_battery_service(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("BATTERY_READ_REQUESTED", timeout=20)
    dut.expect_exact(
        "BATTERY_READ success=1 length=1 level=73 context=loop", timeout=20
    )
    dut.expect_exact("BATTERY_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("BATTERY_SUBSCRIBED success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "BATTERY_SUBSCRIPTION notifications=1 context=loop", timeout=20
    )

    peripheral.write("u")
    peripheral.expect_exact(
        "BATTERY_UPDATED stored=1 notified=1 level=42", timeout=10
    )
    dut.expect_exact("BATTERY_CHANGED valid=1 level=42 context=loop", timeout=20)
    peripheral.expect_exact(
        "BATTERY_SENT success=1 value=42 context=loop", timeout=20
    )

    dut.write("u")
    dut.expect_exact("BATTERY_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("BATTERY_UNSUBSCRIBED success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "BATTERY_SUBSCRIPTION notifications=0 context=loop", timeout=20
    )
    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("DISCONNECTED id=1 context=loop", timeout=20)
