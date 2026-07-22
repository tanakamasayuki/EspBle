import re


def test_persistent_subscribe_restores_on_reconnect(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    # First connection: the application subscribes explicitly.
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("CENTRAL_CONNECTED n=1", timeout=20)

    dut.write("n")
    dut.expect_exact("SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact(
        "SUBSCRIBED success=1 notifications=1 connect=1 context=loop", timeout=20
    )
    peripheral.expect(re.compile(rb"SUBSCRIPTION id=\d+ notifications=1 context=loop"), timeout=20)

    peripheral.write("n")
    peripheral.expect_exact("NOTIFY_REQUESTED", timeout=10)
    dut.expect_exact(
        "RECEIVED value=notify-value count=1 connect=1 context=loop", timeout=20
    )

    # Drop the link. A non-bonded peripheral forgets the subscription, so the
    # notification can only resume if the central re-subscribes on reconnect.
    dut.write("x")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("CENTRAL_DISCONNECTED n=1", timeout=20)
    peripheral.expect_exact("READVERTISING", timeout=20)

    # Second connection: the application does NOT subscribe. persistentSubscriptions
    # restores it automatically, so onSubscribed fires for connect=2 unprompted.
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("CENTRAL_CONNECTED n=2", timeout=20)
    dut.expect_exact(
        "SUBSCRIBED success=1 notifications=1 connect=2 context=loop", timeout=20
    )
    peripheral.expect(re.compile(rb"SUBSCRIPTION id=\d+ notifications=1 context=loop"), timeout=20)

    # The restored subscription delivers notifications without a manual re-subscribe.
    peripheral.write("n")
    peripheral.expect_exact("NOTIFY_REQUESTED", timeout=10)
    dut.expect_exact(
        "RECEIVED value=notify-value count=2 connect=2 context=loop", timeout=20
    )

    dut.write("x")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("CENTRAL_DISCONNECTED n=2", timeout=20)
