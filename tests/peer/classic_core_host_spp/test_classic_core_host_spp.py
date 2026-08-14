import re


def test_spp_interoperates_with_the_core_bluedroid_host(dut, peers):
    """EspBle's Classic host against the Bluedroid host Arduino-ESP32 ships.

    Both boards are ESP32s, but only the DUT runs EspBle's independently built
    host; the peer runs BluetoothSerial on the core's own Bluedroid. Everything
    the two exchange therefore crosses a stack boundary: the SDP record the peer
    resolves, the RFCOMM channel, and the payloads in both directions.
    """
    peer = peers["device"]
    peer.expect_exact("COREPEER_READY", timeout=30)
    ready = dut.expect(
        re.compile(rb"COREHOST_SERVER_READY address=([0-9a-f:]+) heap=(\d+)"),
        timeout=30,
    )
    dut.expect_exact("COREHOST_SERVER_STARTED", timeout=10)
    address = ready.group(1)

    # Channel 0 makes the peer resolve the channel from the EspBle server's
    # service record, so a missing or malformed SDP entry fails here.
    peer.write(b"c" + address + b"\n")
    peer.expect_exact("COREPEER_CONNECT requested=1", timeout=20)
    peer.expect_exact("COREPEER_LINK connected=1", timeout=30)
    connected = dut.expect(
        re.compile(rb"COREHOST_CONNECTED id=(\d+) incoming=1 peer=([0-9a-f:]+)"),
        timeout=30,
    )
    session = connected.group(1)

    # Both payloads carry a zero byte: SPP is a binary pipe, and a stack that
    # treats it as a C string would truncate here.
    peer.write("w\n")
    peer.expect_exact("COREPEER_TX written=4", timeout=10)
    dut.expect(
        re.compile(rb"COREHOST_RX id=" + session + rb" length=4 hex=11002233"),
        timeout=20,
    )

    dut.write("e")
    dut.expect_exact("COREHOST_TX sent=1", timeout=10)
    peer.expect_exact("COREPEER_RX hex=a5005aff", timeout=20)

    # The peer closes the link, which the EspBle server must report as its own
    # session ending rather than leaving a stale session behind.
    peer.write("d\n")
    peer.expect_exact("COREPEER_DISCONNECT requested=1", timeout=10)
    peer.expect_exact("COREPEER_LINK connected=0", timeout=20)
    dut.expect(re.compile(rb"COREHOST_DISCONNECTED id=" + session), timeout=20)
    dut.write("?")
    dut.expect(
        re.compile(rb"COREHOST_STATE server=1 sessions=0 dropped=0 heap=\d+"),
        timeout=10,
    )

    # Reconnect on the same server instance: the service record must still be
    # discoverable and the second session must work exactly like the first.
    peer.write(b"c" + address + b"\n")
    peer.expect_exact("COREPEER_CONNECT requested=1", timeout=20)
    peer.expect_exact("COREPEER_LINK connected=1", timeout=30)
    reconnected = dut.expect(
        re.compile(rb"COREHOST_CONNECTED id=(\d+) incoming=1 peer=[0-9a-f:]+"),
        timeout=30,
    )
    assert reconnected.group(1) != session

    dut.write("e")
    dut.expect_exact("COREHOST_TX sent=1", timeout=10)
    peer.expect_exact("COREPEER_RX hex=a5005aff", timeout=20)
    peer.write("w\n")
    peer.expect_exact("COREPEER_TX written=4", timeout=10)
    dut.expect(
        re.compile(
            rb"COREHOST_RX id=" + reconnected.group(1) + rb" length=4 hex=11002233"
        ),
        timeout=20,
    )

    # Restarting the EspBle stack must leave the peer able to dial in again,
    # which is what a user power-cycling one side of the pair does.
    dut.write("d")
    dut.expect_exact("COREHOST_DISCONNECT requested=1", timeout=10)
    peer.expect_exact("COREPEER_LINK connected=0", timeout=30)
    dut.write("r")
    dut.expect_exact("COREHOST_RESTART started=1", timeout=30)
    restarted = dut.expect(
        re.compile(rb"COREHOST_SERVER_READY address=([0-9a-f:]+) heap=(\d+)"),
        timeout=20,
    )
    assert restarted.group(1) == address

    peer.write(b"c" + address + b"\n")
    peer.expect_exact("COREPEER_CONNECT requested=1", timeout=20)
    peer.expect_exact("COREPEER_LINK connected=1", timeout=30)
    dut.expect(
        re.compile(rb"COREHOST_CONNECTED id=\d+ incoming=1 peer=[0-9a-f:]+"),
        timeout=30,
    )
    peer.write("w\n")
    dut.expect(re.compile(rb"COREHOST_RX id=\d+ length=4 hex=11002233"), timeout=20)

    # A restart that leaks heap would show up across the two ready lines.
    assert int(restarted.group(2)) >= int(ready.group(2)) - 8192
