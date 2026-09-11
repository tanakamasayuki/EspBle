import re


def test_disconnect_reason(dut, peers):
    """EspBleConnection::disconnectReason surfaces the backend/HCI reason code in
    onDisconnected. The peripheral (peer) terminates the link, so as its
    initiator it reports a non-zero local-termination reason, while the central
    (DUT), being the remote side, reports a non-zero remote-termination reason.
    The two codes must differ, proving the reason is captured on both the server
    and client disconnection paths."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect(re.compile(rb"CONNECTED id=(\d+)"), timeout=20)
    device.expect(re.compile(rb"PEER_CONNECTED id=(\d+)"), timeout=20)

    # The peripheral initiates the disconnect.
    device.write("x")
    device.expect_exact("PEER_DISCONNECT_REQUESTED", timeout=10)

    peer_disc = device.expect(
        re.compile(rb"PEER_DISCONNECTED id=(\d+) reason=(-?\d+) context=(\w+)"), timeout=20)
    initiator_reason = int(peer_disc.group(2))
    assert initiator_reason != 0, "initiator (peripheral) reason should be non-zero"
    assert peer_disc.group(3) == b"loop", "must be delivered from update()/loop"

    dut_disc = dut.expect(
        re.compile(rb"DISCONNECTED id=(\d+) reason=(-?\d+) context=(\w+)"), timeout=20)
    remote_reason = int(dut_disc.group(2))
    assert remote_reason != 0, "remote (central) reason should be non-zero"
    assert dut_disc.group(3) == b"loop", "must be delivered from update()/loop"

    assert initiator_reason != remote_reason, (
        "local-termination and remote-termination reason codes should differ "
        f"(initiator={initiator_reason}, remote={remote_reason})")
