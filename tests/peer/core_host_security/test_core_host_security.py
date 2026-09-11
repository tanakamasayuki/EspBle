import re


def test_pairing_and_bonding_interoperate_with_the_core_bluedroid_stack(
    dut, peers, probe
):
    """EspBle pairing against the BLE stack Arduino-ESP32 ships.

    The DUT runs EspBle on a NimBLE host; the peer links no EspBle code and
    pairs through the core's `BLE` wrapper on Bluedroid, clearing its own bonds
    with `esp_ble_remove_bond_device()`. Key exchange, encryption and the
    encrypted-attribute permission therefore cross the stack boundary.
    """
    peer = peers["device"]

    ready = probe(peer, "?\n", re.compile(rb"SECPEER_READY address=([0-9a-f:]+)"))
    peer_address = ready.group(1).decode()
    probe(dut, "?\n", re.compile(rb"SECGATT_READY"))

    # Both sides start from an empty bond list so the run below is a first
    # pairing, not a restore of keys left by an earlier run.
    peer.write("c\n")
    peer.expect(re.compile(rb"SECPEER_BONDS_CLEARED removed=\d+ remaining=0"), timeout=20)
    dut.write("c\n")
    dut.expect(re.compile(rb"SECGATT_BONDS_CLEARED removed=\d+ remaining=0"), timeout=20)

    dut.write("s\n")
    dut.expect_exact("SECGATT_SCAN started=1", timeout=10)
    connect = dut.expect(
        re.compile(rb"SECGATT_CONNECT requested=1 peer=([0-9a-f:]+)"), timeout=30
    )
    assert connect.group(1).decode() == peer_address
    dut.expect(re.compile(rb"SECGATT_CONNECTED id=\d+"), timeout=30)
    peer.expect(re.compile(rb"SECPEER_CONNECTED id=\d+"), timeout=30)

    # Pairing is driven by the DUT's pairOnConnect. Both sides must agree that
    # it succeeded, and the peer reports the auth_mode it settled on so a silent
    # downgrade to an unbonded or legacy pairing fails here.
    dut.expect(
        re.compile(rb"SECGATT_SECURITY success=1 encrypted=1 bonded=1"), timeout=40
    )
    auth = peer.expect(
        re.compile(rb"SECPEER_AUTH success=1 auth_mode=([0-9a-f]{2}) bonds=(\d+)"),
        timeout=40,
    )
    auth_mode = int(auth.group(1), 16)
    assert auth_mode & 0x01, f"expected bonding in auth_mode {auth_mode:#04x}"
    assert auth_mode & 0x08, f"expected secure connections in auth_mode {auth_mode:#04x}"
    assert int(auth.group(2)) >= 1, "peer stored no bond"

    # The characteristic carries ESP_GATT_PERM_READ_ENCRYPTED on the Bluedroid
    # side, so a successful read is evidence that the link is actually
    # encrypted, not merely that both sides claim it is.
    dut.write("r\n")
    dut.expect_exact("SECGATT_READ_REQUESTED 1", timeout=10)
    peer.expect(re.compile(rb"SECPEER_ENCRYPTED_READ count=\d+"), timeout=20)
    dut.expect_exact("SECGATT_READ success=1 value=core-host-secret", timeout=20)

    # Reconnecting must restore encryption from the stored keys without a new
    # pairing exchange: the peer's bond count stays as it was.
    peer.write("d\n")
    peer.expect_exact("SECPEER_DISCONNECT requested=1", timeout=10)
    dut.expect(re.compile(rb"SECGATT_DISCONNECTED reason=\d+"), timeout=20)
    dut.write("a\n")
    dut.expect_exact("SECGATT_REARMED", timeout=10)
    dut.write("s\n")
    dut.expect_exact("SECGATT_SCAN started=1", timeout=10)
    dut.expect(re.compile(rb"SECGATT_CONNECTED id=\d+"), timeout=30)
    dut.expect(
        re.compile(rb"SECGATT_SECURITY success=1 encrypted=1 bonded=1"), timeout=40
    )
    dut.write("r\n")
    dut.expect_exact("SECGATT_READ_REQUESTED 1", timeout=10)
    dut.expect_exact("SECGATT_READ success=1 value=core-host-secret", timeout=20)

    state = probe(
        dut,
        "?\n",
        re.compile(rb"SECGATT_STATE connected=1 encrypted=1 bonded=1 bonds=(\d+)"),
    )
    assert int(state.group(1)) >= 1, "DUT kept no bond for the peer"

    peer_state = probe(
        peer,
        "?\n",
        re.compile(rb"SECPEER_STATE connected=1 authenticated=\d+ bonded=\d+ bonds=(\d+)"),
    )
    assert int(peer_state.group(1)) >= 1, "peer kept no bond for the DUT"
