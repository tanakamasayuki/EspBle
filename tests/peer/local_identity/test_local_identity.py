import re

OBSERVED = re.compile(rb"OBSERVED address=(\S+) type=(\d+) txpower=(\S+)")
LOCAL = re.compile(rb"LOCAL_ADDRESS (\S+) type=(\d+)")


def _observe(dut, command="s"):
    dut.write(command)
    dut.expect_exact("SCAN_STARTED", timeout=10)
    match = dut.expect(OBSERVED, timeout=20)
    return match.group(1).decode().lower(), int(match.group(2)), match.group(3).decode()


def test_local_identity(dut, peers):
    peripheral = peers["device"]

    # localAddress() must be the address the peer actually transmits.
    peripheral.write("a")
    match = peripheral.expect(LOCAL, timeout=10)
    reported_address = match.group(1).decode().lower()
    reported_type = int(match.group(2))
    assert reported_address != "", "localAddress() returned nothing"
    # Public was requested, so localAddressType() must say Public (0).
    assert reported_type == 0, f"unexpected local address type {reported_type}"

    observed_address, observed_type, _ = _observe(dut)
    assert observed_address == reported_address, (
        f"localAddress() {reported_address} != observed {observed_address}"
    )
    assert observed_type == reported_type

    # setTxPower() must change the level the radio applies. The advertised Tx
    # Power Level is filled in by the controller, so the change is visible over
    # the air.
    peripheral.write("l")
    peripheral.expect_exact("SET_TX_POWER 1", timeout=10)
    peripheral.expect_exact("TX_POWER -12", timeout=10)
    _, _, low_observed = _observe(dut)
    assert low_observed == "-12", f"advertised Tx Power {low_observed}, expected -12"

    peripheral.write("h")
    peripheral.expect_exact("SET_TX_POWER 1", timeout=10)
    peripheral.expect_exact("TX_POWER 9", timeout=10)
    _, _, high_observed = _observe(dut)
    assert high_observed == "9", f"advertised Tx Power {high_observed}, expected 9"

    # The reason code passed to disconnect() must reach the peer.
    _observe(dut, "c")
    dut.expect("CENTRAL_CONNECTED id=", timeout=20)
    peripheral.expect("PERIPHERAL_CONNECTED id=", timeout=10)

    # The peer disconnects with 0x13 (remote user terminated). Only the reasons
    # the Core specification lists for HCI_Disconnect may be sent by a host; the
    # original ESP32's controller rejects anything else, so the peer would fail
    # the request instead of terminating the link.
    peripheral.write("d")
    peripheral.expect_exact("DISCONNECT 1", timeout=10)
    dut.expect_exact("reason=0x13", timeout=15)
