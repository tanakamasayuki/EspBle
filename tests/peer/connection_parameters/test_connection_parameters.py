import re


def test_connection_parameters(dut, peers):
    """EspBleConnection exposes the current connection parameters, and
    updateConnectionParameters() requests an update whose negotiated result is
    delivered to onConnectionParametersUpdated() on both peers. The central
    requests interval 80 (100 ms); both the central and the peripheral report the
    updated interval of 80."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)

    connected = dut.expect(re.compile(
        rb"CONNECTED id=(\d+) interval=(\d+) latency=(\d+) timeout=(\d+) context=(\w+)"), timeout=20)
    assert int(connected.group(2)) > 0, "initial connection interval should be populated"
    assert connected.group(5) == b"loop"
    device.expect(re.compile(rb"PEER_CONNECTED id=(\d+) interval=(\d+)"), timeout=20)

    # The central requests a connection parameter update to interval 80.
    dut.write("p")
    dut.expect_exact("UPDATE_REQUESTED", timeout=10)

    updated = dut.expect(re.compile(
        rb"PARAMS_UPDATED interval=(\d+) latency=(\d+) timeout=(\d+) context=(\w+)"), timeout=20)
    assert int(updated.group(1)) == 80, "central should report the negotiated interval 80"
    assert int(updated.group(3)) == 200, "central should report the negotiated supervision timeout 200"
    assert updated.group(4) == b"loop", "must be delivered from update()/loop"

    peer_updated = device.expect(re.compile(
        rb"PEER_PARAMS_UPDATED interval=(\d+) latency=(\d+) timeout=(\d+) context=(\w+)"), timeout=20)
    assert int(peer_updated.group(1)) == 80, "peripheral should report the same negotiated interval 80"
    assert peer_updated.group(4) == b"loop"

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
