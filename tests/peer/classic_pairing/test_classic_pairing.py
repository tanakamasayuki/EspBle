import re


def test_numeric_comparison_pairing_and_bond_management(dut, peers):
    """Classic pairing driven by the application instead of auto-accepted.

    Both boards run DisplayYesNo, so the controllers produce the same six-digit
    number on each side and neither can proceed until its sketch answers. That
    is the property that makes the pairing meaningful: an implementation that
    silently accepts would pass a connection test but fail the comparison
    checks here.
    """
    peer = peers["device"]
    peer_ready = peer.expect(
        re.compile(rb"PAIR_PEER_READY address=([0-9a-f:]+)"), timeout=30
    )
    ready = dut.expect(re.compile(rb"PAIR_READY address=([0-9a-f:]+)"), timeout=30)
    dut_address = ready.group(1).decode()
    peer_address = peer_ready.group(1).decode()

    # Start from no bonds so the pairing below is a first pairing, not a
    # reconnection using keys left over from an earlier run.
    for device, prefix, newline in ((dut, "PAIR", ""), (peer, "PAIR_PEER", "\n")):
        device.write("x" + newline)
        device.expect(
            re.compile(prefix.encode() + rb"_DELETE_ALL removed=\d+ count=0"),
            timeout=20,
        )

    # Answering when nothing is pending must fail: a stale answer would confirm
    # a pairing the user never saw.
    dut.write("w")
    dut.expect_exact("PAIR_STALE accepted=0 error=InvalidState", timeout=10)

    peer.write("c" + dut_address + "\n")
    peer.expect_exact("PAIR_PEER_CONNECT requested=1", timeout=20)

    # Both sides are asked, and both must be shown the same number.
    dut_compare = dut.expect(
        re.compile(rb"PAIR_COMPARE peer=([0-9a-f:]+) value=(\d{6}) auto=1"),
        timeout=40,
    )
    peer_compare = peer.expect(
        re.compile(rb"PAIR_PEER_COMPARE peer=([0-9a-f:]+) value=(\d{6}) auto=1"),
        timeout=40,
    )
    assert dut_compare.group(1).decode() == peer_address
    assert peer_compare.group(1).decode() == dut_address
    assert dut_compare.group(2) == peer_compare.group(2)
    dut.expect_exact("PAIR_CONFIRM accepted=1", timeout=10)
    peer.expect_exact("PAIR_PEER_CONFIRM accepted=1", timeout=10)

    dut.expect(
        re.compile(rb"PAIR_SECURITY peer=" + peer_address.encode() +
                   rb" success=1 status=0"),
        timeout=40,
    )
    peer.expect(
        re.compile(rb"PAIR_PEER_SECURITY peer=" + dut_address.encode() +
                   rb" success=1 status=0"),
        timeout=40,
    )
    dut.expect(re.compile(rb"PAIR_CONNECTED id=\d+ peer=" + peer_address.encode()),
               timeout=30)

    # The bond both sides just created must be listable by address.
    dut.write("b")
    dut.expect_exact("PAIR_BONDS count=1 " + peer_address, timeout=10)
    peer.write("b\n")
    peer.expect_exact("PAIR_PEER_BONDS count=1 " + dut_address, timeout=10)

    # A rejected comparison must not produce a bond. Drop the keys on both
    # sides first, otherwise the peers reconnect from the existing bond and
    # never ask again.
    dut.write("d")
    dut.expect_exact("PAIR_DISCONNECT requested=1", timeout=10)
    peer.expect(re.compile(rb"PAIR_PEER_DISCONNECTED id=\d+"), timeout=20)
    for device, prefix, newline in ((dut, "PAIR", ""), (peer, "PAIR_PEER", "\n")):
        device.write("x" + newline)
        device.expect(
            re.compile(prefix.encode() + rb"_DELETE_ALL removed=\d+ count=0"),
            timeout=20,
        )

    dut.write("n")
    dut.expect_exact("PAIR_AUTO off", timeout=10)
    peer.write("c" + dut_address + "\n")
    peer.expect_exact("PAIR_PEER_CONNECT requested=1", timeout=20)
    dut.expect(
        re.compile(rb"PAIR_COMPARE peer=[0-9a-f:]+ value=\d{6} auto=0"), timeout=40
    )
    dut.write("r")
    dut.expect_exact("PAIR_REJECT rejected=1 error=None", timeout=10)
    dut.expect(
        re.compile(rb"PAIR_SECURITY peer=[0-9a-f:]+ success=0 status=\d+"),
        timeout=40,
    )
    dut.write("b")
    dut.expect_exact("PAIR_BONDS count=0", timeout=10)

    # The side that asked for the connection must be told it failed. The
    # backend sends no SPP event when pairing is what failed, so without an
    # explicit report the attempt would stay in flight until its own timeout
    # and the retry below would be refused.
    peer.expect(
        re.compile(rb"PAIR_PEER_CONNECT_FAILED peer=[0-9a-f:]+ detail=\S"),
        timeout=20,
    )

    # After a rejection the stack must still pair normally, so the rejection
    # left no half-finished state behind.
    dut.write("y")
    dut.expect_exact("PAIR_AUTO on", timeout=10)
    peer.write("c" + dut_address + "\n")
    peer.expect_exact("PAIR_PEER_CONNECT requested=1", timeout=20)
    dut.expect(
        re.compile(rb"PAIR_COMPARE peer=[0-9a-f:]+ value=\d{6} auto=1"), timeout=40
    )
    dut.expect(
        re.compile(rb"PAIR_SECURITY peer=[0-9a-f:]+ success=1 status=0"), timeout=40
    )
    dut.expect(re.compile(rb"PAIR_CONNECTED id=\d+ peer=[0-9a-f:]+"), timeout=30)
    dut.write("b")
    dut.expect_exact("PAIR_BONDS count=1 " + peer_address, timeout=10)

    # Deleting the bond by value must remove exactly that entry.
    dut.write("x")
    dut.expect(re.compile(rb"PAIR_DELETE_ALL removed=1 count=0"), timeout=20)
    dut.write("?")
    dut.expect(re.compile(rb"PAIR_STATE sessions=\d+ bonds=0 heap=\d+"), timeout=10)
