import re


def test_phy_update(dut, peers):
    """EspBleConnection exposes the current LE PHY (txPhy/rxPhy), and updatePhy()
    requests a PHY change whose negotiated result is delivered to onPhyUpdated()
    on both peers. The central requests the 2M PHY; both the central and the
    peripheral report tx/rx PHY 2 (ESP32-S3 supports the 2M PHY)."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)

    connected = dut.expect(re.compile(
        rb"CONNECTED id=(\d+) tx_phy=(\d+) rx_phy=(\d+) context=(\w+)"), timeout=20)
    assert int(connected.group(2)) > 0, "initial tx PHY should be populated"
    assert int(connected.group(3)) > 0, "initial rx PHY should be populated"
    assert connected.group(4) == b"loop"
    device.expect(re.compile(rb"PEER_CONNECTED id=(\d+) tx_phy=(\d+) rx_phy=(\d+)"), timeout=20)

    # The central requests the 2M PHY in both directions.
    dut.write("p")
    dut.expect_exact("PHY_REQUESTED", timeout=10)

    updated = dut.expect(re.compile(
        rb"PHY_UPDATED tx_phy=(\d+) rx_phy=(\d+) context=(\w+)"), timeout=20)
    assert int(updated.group(1)) == 2, "central should report the negotiated 2M tx PHY"
    assert int(updated.group(2)) == 2, "central should report the negotiated 2M rx PHY"
    assert updated.group(3) == b"loop", "must be delivered from update()/loop"

    peer_updated = device.expect(re.compile(
        rb"PEER_PHY_UPDATED tx_phy=(\d+) rx_phy=(\d+) context=(\w+)"), timeout=20)
    assert int(peer_updated.group(1)) == 2, "peripheral should report the negotiated 2M tx PHY"
    assert int(peer_updated.group(2)) == 2, "peripheral should report the negotiated 2M rx PHY"
    assert peer_updated.group(3) == b"loop"

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
