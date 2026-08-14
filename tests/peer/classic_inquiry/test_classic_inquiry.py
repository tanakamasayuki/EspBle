import re


def test_inquiry_finds_the_peer_and_reports_completion(dut, peers):
    """Classic device discovery, the entry point for every address-based API.

    The peer only makes itself discoverable; everything asserted here is what
    the scanning side observes, so a peer that answers twice does not change
    the outcome.
    """
    peer = peers["device"]
    peer_ready = peer.expect(
        re.compile(rb"INQUIRY_PEER_READY address=([0-9a-f:]+)"), timeout=30
    )
    dut.expect(re.compile(rb"INQUIRY_READY address=[0-9a-f:]+"), timeout=30)
    peer_address = peer_ready.group(1).decode()

    # A duration of 0 cannot be encoded for the controller, so it must be
    # refused locally instead of starting a scan that never ends.
    dut.write("z")
    dut.expect_exact("INQUIRY_INVALID started=0 error=InvalidArgument", timeout=10)

    dut.write("s")
    dut.expect_exact("INQUIRY_START started=1 running=1", timeout=10)

    # A second scan while one is running must be refused rather than queued:
    # the controller has one inquiry state, not a queue of them.
    dut.write("a")
    dut.expect_exact("INQUIRY_RESTART started=0 error=InvalidState", timeout=10)

    found = dut.expect(
        re.compile(
            # The name is a device name, so it contains spaces; only the
            # following key ends it.
            rb"INQUIRY_RESULT address=" + peer_address.encode() +
            rb" name=(.+?) cod=(\d):([0-9a-f]+) rssi=(\d):(-?\d+)"
        ),
        timeout=40,
    )
    # The peer sets a device name, and an ESP32 always reports its class of
    # device, so both must reach the caller rather than being dropped in
    # decoding. RSSI is optional in the inquiry result, so it is not required.
    assert found.group(1) == b"EspBle Inquiry Peer"
    assert found.group(2) == b"1"
    assert int(found.group(3), 16) != 0

    # The scan ends on its own once the duration elapses; five seconds is
    # rounded up to the controller's 1.28 s units.
    completed = dut.expect(
        re.compile(rb"INQUIRY_COMPLETE cancelled=0 results=(\d+) dropped=(\d+)"),
        timeout=40,
    )
    assert int(completed.group(1)) >= 1
    assert completed.group(2) == b"0"
    dut.write("?")
    dut.expect(re.compile(rb"INQUIRY_STATE running=0 results=\d+ completes=1 heap=\d+"),
               timeout=10)

    # Cancelling a long scan must complete promptly and say it was cancelled,
    # so a sketch can tell the two endings apart.
    dut.write("l")
    dut.expect_exact("INQUIRY_START started=1 running=1", timeout=10)
    dut.write("x")
    dut.expect_exact("INQUIRY_STOP requested=1 error=None", timeout=10)
    dut.expect(
        re.compile(rb"INQUIRY_COMPLETE cancelled=1 results=\d+ dropped=\d+"),
        timeout=20,
    )
    dut.write("?")
    dut.expect(re.compile(rb"INQUIRY_STATE running=0 .*"), timeout=10)

    # Stopping when nothing runs is a caller mistake, not a silent no-op.
    dut.write("x")
    dut.expect_exact("INQUIRY_STOP requested=0 error=InvalidState", timeout=10)

    # The scanner must still be usable after a cancellation.
    dut.write("s")
    dut.expect_exact("INQUIRY_START started=1 running=1", timeout=10)
    dut.expect(
        re.compile(rb"INQUIRY_RESULT address=" + peer_address.encode() + rb" .*"),
        timeout=40,
    )
    dut.expect(
        re.compile(rb"INQUIRY_COMPLETE cancelled=0 results=\d+ dropped=0"),
        timeout=40,
    )
