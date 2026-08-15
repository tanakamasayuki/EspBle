import re


def test_gatt_interoperates_with_the_core_bluedroid_stack(dut, peers, probe):
    """EspBle's GATT client against the BLE stack Arduino-ESP32 ships.

    The DUT runs EspBle on a NimBLE host; the peer links no EspBle code and runs
    the core's `BLE` wrapper, which is Bluedroid on the original ESP32. Two
    EspBle boards can share a misunderstanding and still pass; here every step
    crosses a stack boundary, so only what the specification requires of both
    sides is asserted, and both sides are checked.
    """
    peer = peers["device"]

    ready = probe(
        peer, "?\n", re.compile(rb"COREPEER_READY address=([0-9a-f:]+)")
    )
    peer_address = ready.group(1).decode()
    probe(dut, "?\n", re.compile(rb"COREGATT_READY"))

    # Discovery of the advertisement itself is the first cross-stack step: the
    # DUT connects only to a peer whose advertising data carries the service
    # UUID the Bluedroid side put there.
    dut.write("s\n")
    dut.expect_exact("COREGATT_SCAN started=1", timeout=10)
    connect = dut.expect(
        re.compile(rb"COREGATT_CONNECT requested=1 peer=([0-9a-f:]+)"), timeout=30
    )
    assert connect.group(1).decode() == peer_address, (
        "connected to a different address than the peer reported"
    )
    dut.expect(re.compile(rb"COREGATT_CONNECTED id=\d+ mtu=\d+"), timeout=30)
    peer.expect(re.compile(rb"COREPEER_CONNECTED id=\d+"), timeout=30)

    # The attribute table was built by Bluedroid and is read by NimBLE: three
    # characteristics, and the notify characteristic's CCCD must be visible to
    # the client or subscribing below could not work.
    discovered = dut.expect(
        re.compile(rb"COREGATT_DISCOVERED success=1 chars=(\d+) notify_descs=(\d+)"),
        timeout=30,
    )
    assert int(discovered.group(1)) == 3, "expected three characteristics"
    assert int(discovered.group(2)) >= 1, "expected the notify CCCD to be discovered"

    dut.write("r\n")
    dut.expect_exact("COREGATT_READ_REQUESTED 1", timeout=10)
    peer.expect_exact("COREPEER_READ_SERVED", timeout=20)
    dut.expect_exact("COREGATT_READ success=1 value=core-host-value", timeout=20)

    # The payload contains 0x00. A stack that treats an attribute value as a C
    # string truncates it, and the peer prints hex so the test can tell.
    dut.write("w\n")
    dut.expect_exact("COREGATT_WRITE_REQUESTED 1", timeout=10)
    peer.expect_exact("COREPEER_WRITTEN length=4 hex=11002233", timeout=20)
    dut.expect_exact("COREGATT_WROTE success=1 response=1", timeout=20)

    # Subscribing writes the CCCD; the peer reports the raw bits it received, so
    # a client that writes the wrong value shows up here rather than as a
    # missing notification later.
    dut.write("n\n")
    dut.expect_exact("COREGATT_SUBSCRIBE_REQUESTED 1", timeout=10)
    peer.expect_exact("COREPEER_CCCD name=notify value=0001", timeout=20)
    dut.expect(re.compile(rb"COREGATT_SUBSCRIBED success=1"), timeout=20)

    peer.write("n\n")
    peer.expect(re.compile(rb"COREPEER_NOTIFIED hex=a5005a01"), timeout=20)
    dut.expect(
        re.compile(rb"COREGATT_NOTIFY uuid=\S+ length=4 hex=a5005a01 indication=0"),
        timeout=20,
    )

    # Indications take the other CCCD bit and must arrive flagged as such.
    dut.write("i\n")
    dut.expect_exact("COREGATT_INDICATE_SUBSCRIBE_REQUESTED 1", timeout=10)
    peer.expect_exact("COREPEER_CCCD name=indicate value=0002", timeout=20)
    peer.write("i\n")
    peer.expect_exact("COREPEER_INDICATED hex=0f00f0", timeout=20)
    dut.expect(
        re.compile(rb"COREGATT_NOTIFY uuid=\S+ length=3 hex=0f00f0 indication=1"),
        timeout=20,
    )

    # Unsubscribing has to clear the CCCD on the server, not just stop local
    # delivery: the peer reports the value it now holds.
    dut.write("u\n")
    dut.expect_exact("COREGATT_UNSUBSCRIBE_REQUESTED 1", timeout=10)
    peer.expect_exact("COREPEER_CCCD name=notify value=0000", timeout=20)

    # A value set after discovery must be read through the same handles.
    peer.write("v\n")
    peer.expect_exact("COREPEER_VALUE_SET core-host-second", timeout=10)
    dut.write("r\n")
    dut.expect_exact("COREGATT_READ_REQUESTED 1", timeout=10)
    dut.expect_exact("COREGATT_READ success=1 value=core-host-second", timeout=20)

    # The Bluedroid side closes the link; the NimBLE side must report it as a
    # disconnect of its own connection rather than leaving a stale one.
    peer.write("d\n")
    peer.expect_exact("COREPEER_DISCONNECT requested=1", timeout=10)
    peer.expect_exact("COREPEER_DISCONNECTED", timeout=20)
    dut.expect(re.compile(rb"COREGATT_DISCONNECTED reason=\d+"), timeout=20)
    state = probe(
        dut, "?\n", re.compile(rb"COREGATT_STATE connected=(\d+) discovered=(\d+)")
    )
    assert state.group(1) == b"0" and state.group(2) == b"0"

    # Reconnecting exercises the peer's advertising restart and a fresh
    # discovery against the same Bluedroid attribute table.
    dut.write("a\n")
    dut.expect_exact("COREGATT_REARMED", timeout=10)
    dut.write("s\n")
    dut.expect_exact("COREGATT_SCAN started=1", timeout=10)
    dut.expect(re.compile(rb"COREGATT_CONNECTED id=\d+ mtu=\d+"), timeout=30)
    peer.expect(re.compile(rb"COREPEER_CONNECTED id=\d+"), timeout=30)
    rediscovered = dut.expect(
        re.compile(rb"COREGATT_DISCOVERED success=1 chars=(\d+) notify_descs=\d+"),
        timeout=30,
    )
    assert int(rediscovered.group(1)) == 3
