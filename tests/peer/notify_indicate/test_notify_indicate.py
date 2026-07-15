def test_notify_indicate(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("CENTRAL_CONNECTED id=1", timeout=20)

    dut.write("n")
    dut.expect_exact("SUBSCRIBE_NOTIFY_REQUESTED", timeout=10)
    dut.expect_exact(
        "SUBSCRIBED success=1 notifications=1 indications=0 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "SUBSCRIPTION id=1 notifications=1 indications=0 context=loop",
        timeout=20,
    )

    peripheral.write("n")
    peripheral.expect_exact("NOTIFY_REQUESTED", timeout=10)
    dut.expect_exact(
        "RECEIVED id=1 indication=0 value=notify-value context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "SENT indication=0 success=1 value=notify-value detail= context=loop",
        timeout=20,
    )

    dut.write("u")
    dut.expect_exact("UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("UNSUBSCRIBED success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "SUBSCRIPTION id=1 notifications=0 indications=0 context=loop",
        timeout=20,
    )

    dut.write("i")
    dut.expect_exact("SUBSCRIBE_INDICATE_REQUESTED", timeout=10)
    dut.expect_exact(
        "SUBSCRIBED success=1 notifications=0 indications=1 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "SUBSCRIPTION id=1 notifications=0 indications=1 context=loop",
        timeout=20,
    )

    peripheral.write("i")
    peripheral.expect_exact("INDICATE_REQUESTED", timeout=10)
    dut.expect_exact(
        "RECEIVED id=1 indication=1 value=indicate-value context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "SENT indication=1 success=1 value=indicate-value detail= context=loop",
        timeout=20,
    )

    dut.write("u")
    dut.expect_exact("UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("UNSUBSCRIBED success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "SUBSCRIPTION id=1 notifications=0 indications=0 context=loop",
        timeout=20,
    )
