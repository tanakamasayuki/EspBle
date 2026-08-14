import re


def test_spp_stream_adapter_behaves_like_a_stream(dut, peers, probe):
    """SPP through the Arduino Stream adapter, measured by the session API.

    The peer opens the session and counts what arrives with the plain API, so
    nothing here is the adapter checking itself. The checksum is order-sensitive:
    a buffer larger than one SPP packet has to come out the other side split into
    packets but in order.
    """
    peer = peers["device"]
    probe(peer, "?\n", re.compile(rb"PEER_STATE session=0 bytes=0 "))
    ready = probe(dut, "a\n", re.compile(rb"ADDRESS ([0-9a-f:]+)"))
    address = ready.group(1).decode()

    peer.write("c" + address + "\n")
    peer.expect_exact("PEER_CONNECT requested=1", timeout=20)
    peer.expect(re.compile(rb"PEER_CONNECTED peer=[0-9a-f:]+"), timeout=40)
    dut.expect(re.compile(rb"STREAM_CONNECTED peer=[0-9a-f:]+ session=\d+"), timeout=20)

    # The Stream borrows the session rather than owning one, so being attached
    # and being connected are separate facts.
    dut.write("?\n")
    dut.expect(
        re.compile(rb"STREAM_STATE attached=1 connected=1 session=\d+ avail=0"),
        timeout=10,
    )

    # println() goes through Print::write(buffer, size) and adds CR LF, so 14
    # bytes leave for 12 characters.
    peer.write("z\n")
    peer.expect_exact("PEER_RESET", timeout=10)
    dut.write("l\n")
    dut.expect_exact("STREAM_PRINT written=14", timeout=10)
    peer.write("?\n")
    peer.expect(
        re.compile(
            rb"PEER_STATE session=\d+ bytes=14 checksum=677446872 line=hello stream"
        ),
        timeout=20,
    )

    # 2500 bytes is more than one 990-byte packet, so the adapter splits it. The
    # checksum fails if the pieces arrive out of order or a boundary byte is lost.
    peer.write("z\n")
    peer.expect_exact("PEER_RESET", timeout=10)
    dut.write("b\n")
    dut.expect_exact("STREAM_BULK requested=2500 written=2500", timeout=20)
    dut.write("f\n")
    flushed = dut.expect(
        re.compile(rb"STREAM_FLUSH pending=0 elapsed=(\d+)"), timeout=20
    )
    assert int(flushed.group(1)) < 1000, flushed.group(1)
    peer.write("?\n")
    peer.expect(
        re.compile(rb"PEER_STATE session=\d+ bytes=2500 checksum=647092539"),
        timeout=20,
    )

    # With the write timeout at zero a write that does not fit reports what it
    # took instead of stalling: 12 packets asked for, a queue that holds 8.
    dut.write("n\n")
    nowait = dut.expect(
        re.compile(rb"STREAM_NOWAIT requested=11880 written=(\d+) elapsed=(\d+)"),
        timeout=20,
    )
    written = int(nowait.group(1))
    assert 0 < written < 11880, written
    assert int(nowait.group(2)) < 200, nowait.group(2)
    dut.write("f\n")
    dut.expect(re.compile(rb"STREAM_FLUSH pending=0 elapsed=\d+"), timeout=20)

    # The read side: readStringUntil() with the Stream timeout, then parseInt(),
    # which needs nothing but read() and peek() and so proves the adapter is a
    # Stream rather than a lookalike.
    peer.write("z\n")
    peer.expect_exact("PEER_RESET", timeout=10)
    peer.write("wfrom the peer\n")
    peer.expect(re.compile(rb"PEER_WROTE accepted=1 len=\d+"), timeout=10)
    dut.write("r\n")
    dut.expect_exact("STREAM_LINE len=13 value=from the peer", timeout=20)

    peer.write("w4242\n")
    peer.expect(re.compile(rb"PEER_WROTE accepted=1 len=\d+"), timeout=10)
    dut.write("i\n")
    dut.expect_exact("STREAM_INT value=4242", timeout=20)

    # Detaching leaves the session open — the SPP API still owns it — and makes
    # the Stream inert instead of writing to a session it no longer names.
    dut.write("d\n")
    dut.expect_exact("STREAM_DETACHED attached=0 avail=0", timeout=10)
    dut.write("l\n")
    dut.expect_exact("STREAM_PRINT written=0", timeout=10)
    peer.write("?\n")
    peer.expect(re.compile(rb"PEER_STATE session=\d+ bytes=0 "), timeout=20)

    peer.write("d\n")
    peer.expect_exact("PEER_DISCONNECTED", timeout=20)
    dut.expect_exact("STREAM_DISCONNECTED", timeout=20)
