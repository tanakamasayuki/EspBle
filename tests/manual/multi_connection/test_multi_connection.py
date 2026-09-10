import re


def test_multi_connection_and_auto_reconnect(dut, peers):
    # peers["device"] is Peripheral A; peers["device2"] is Peripheral B (the
    # third board). The pytest is skipped automatically when device2's port is
    # not configured, so this only runs with all three boards attached.
    peer_a = peers["device"]
    peer_b = peers["device2"]

    peer_a.write("?")
    peer_a.expect_exact("ADVERTISING 1", timeout=10)
    peer_b.write("?")
    peer_b.expect_exact("ADVERTISING 1", timeout=10)

    # Connect Peripheral A and subscribe.
    dut.write("1")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED label=A ok=1", timeout=20)
    dut.expect_exact("CENTRAL_CONNECTED label=A id=1 count=1", timeout=20)
    peer_a.expect_exact("PEER_A_CONNECTED", timeout=20)
    dut.write("p")
    dut.expect_exact("SUBSCRIBE_A_REQUESTED", timeout=10)
    dut.expect_exact("SUBSCRIBED label=A success=1", timeout=20)

    # Connect Peripheral B while A stays connected: two simultaneous connections.
    dut.write("2")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED label=B ok=1", timeout=20)
    dut.expect_exact("CENTRAL_CONNECTED label=B id=2 count=2", timeout=20)
    peer_b.expect_exact("PEER_B_CONNECTED", timeout=20)
    dut.write("q")
    dut.expect_exact("SUBSCRIBE_B_REQUESTED", timeout=10)
    dut.expect_exact("SUBSCRIBED label=B success=1", timeout=20)

    # Notifications from both peers route to the correct connection.
    peer_a.write("n")
    dut.expect_exact("RECEIVED label=A value=A-notify", timeout=20)
    peer_b.write("n")
    dut.expect_exact("RECEIVED label=B value=B-notify", timeout=20)

    # Drop A from the peripheral side (unexpected for the central). B stays up.
    peer_a.write("x")
    peer_a.expect_exact("PEER_A_DROP_REQUESTED", timeout=10)
    dut.expect_exact("CENTRAL_DISCONNECTED label=A count=1", timeout=20)
    peer_a.expect_exact("PEER_A_READVERTISING", timeout=20)

    # Auto-reconnect restores A (new connection id) back to two connections, and
    # the persistent subscription is restored without a manual re-subscribe.
    dut.expect(re.compile(rb"CENTRAL_CONNECTED label=A id=\d+ count=2"), timeout=40)
    peer_a.expect_exact("PEER_A_CONNECTED", timeout=20)
    dut.expect_exact("SUBSCRIBED label=A success=1", timeout=20)

    # The restored subscription delivers notifications again, and B is unaffected.
    peer_a.write("n")
    dut.expect_exact("RECEIVED label=A value=A-notify", timeout=20)
    peer_b.write("n")
    dut.expect_exact("RECEIVED label=B value=B-notify", timeout=20)
