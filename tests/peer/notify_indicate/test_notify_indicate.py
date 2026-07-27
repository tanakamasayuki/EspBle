def test_notify_indicate(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("CENTRAL_CONNECTED id=1", timeout=20)
    peripheral.expect_exact("PERIPHERAL_CONNECTED id=1", timeout=20)

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
    # Multi-listener core: the second client listener also receives the value.
    dut.expect_exact("RECEIVED2 value=notify-value", timeout=20)
    # Broadcast send reports connectionId 0.
    peripheral.expect_exact(
        "SENT id=0 indication=0 success=1 value=notify-value detail= context=loop",
        timeout=20,
    )
    # ...and the second server sent-listener fires alongside the primary onSent.
    peripheral.expect_exact("SENT2 value=notify-value", timeout=20)

    # Send FIFO: three notifies queued in one loop iteration all succeed and are
    # delivered in order (before the queue, only the first would have been sent).
    peripheral.write("q")
    peripheral.expect_exact("BURST_QUEUED ok=3", timeout=10)
    for value in ("burst-1", "burst-2", "burst-3"):
        dut.expect_exact(
            f"RECEIVED id=1 indication=0 value={value} context=loop",
            timeout=20,
        )
        peripheral.expect_exact(
            f"SENT id=0 indication=0 success=1 value={value} detail= context=loop",
            timeout=20,
        )

    # Connection-scoped notify: targets one connection and reports its id.
    peripheral.write("t")
    peripheral.expect_exact("TARGETED_REQUESTED", timeout=10)
    dut.expect_exact(
        "RECEIVED id=1 indication=0 value=targeted-value context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "SENT id=1 indication=0 success=1 value=targeted-value detail= context=loop",
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
        "SENT id=0 indication=1 success=1 value=indicate-value detail= context=loop",
        timeout=20,
    )

    dut.write("u")
    dut.expect_exact("UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("UNSUBSCRIBED success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "SUBSCRIPTION id=1 notifications=0 indications=0 context=loop",
        timeout=20,
    )
