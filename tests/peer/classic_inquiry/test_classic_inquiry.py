import re


def test_inquiry_finds_the_peer_and_reports_completion(dut, peers):
    """Classic device discovery, the entry point for every address-based API.

    The peer only makes itself discoverable; everything asserted here is what
    the scanning side observes, so a peer that answers twice does not change
    the outcome.
    """
    peer = peers["device"]
    peer_ready = peer.expect(
        re.compile(rb"INQUIRY_PEER_READY address=([0-9a-f:]+) visibility=2"),
        timeout=30,
    )
    dut.expect(re.compile(rb"INQUIRY_READY address=[0-9a-f:]+"), timeout=30)
    peer_address = peer_ready.group(1).decode()

    # The class the peer asked for only reaches the air once its service
    # registration is done and the library has restored it, so the scan below
    # would otherwise race the default value.
    peer.expect_exact("INQUIRY_PEER_COD_LIVE 05:10:000", timeout=20)

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
    # The peer asked to be a Peripheral / keyboard, and that has to be what the
    # scanning side observes over the air: this is the value a Host uses to pick
    # an icon and, on some Hosts, to decide whether to offer connecting at all.
    # Bits 8..12 are the major class and bits 2..7 the minor class.
    observed = int(found.group(3), 16)
    assert (observed >> 8) & 0x1f == 0x05
    assert observed & 0xfc == 0x10 << 2

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


    # --- visibility and Class of Device -----------------------------------
    #
    # Both are properties of the side being looked for, so the peer sets them
    # and this scanning side is the judge. A profile used to decide them when it
    # started, which left a sketch no way to keep a device unlisted or to name
    # what kind of device it is.

    # A class chosen after begin(): a composed device only knows what it is once
    # the sketch has decided which profiles to start.
    peer.write("c\n")
    peer.expect_exact("INQUIRY_PEER_COD accepted=1", timeout=10)
    peer.expect_exact("INQUIRY_PEER_COD_CHANGED 04:05:100", timeout=20)

    dut.write("s")
    dut.expect_exact("INQUIRY_START started=1 running=1", timeout=10)
    found = dut.expect(
        re.compile(
            rb"INQUIRY_RESULT address=" + peer_address.encode() +
            rb" name=(.+?) cod=1:([0-9a-f]+) rssi="
        ),
        timeout=40,
    )
    observed = int(found.group(2), 16)
    assert (observed >> 8) & 0x1f == 0x04
    assert observed & 0xfc == 0x05 << 2
    assert (observed >> 13) & 0x7ff == 0x100  # Audio service bit
    dut.expect(re.compile(rb"INQUIRY_COMPLETE cancelled=0 "), timeout=40)

    # Out-of-range fields are refused rather than truncated, which would
    # silently advertise a different device class.
    peer.write("x\n")
    peer.expect_exact(
        "INQUIRY_PEER_COD_INVALID changed=0 error=InvalidArgument", timeout=10)

    # Hidden while the SPP server keeps running: the profile no longer decides
    # this. Two scans in a row must both come up empty for this peer.
    peer.write("h\n")
    peer.expect_exact("INQUIRY_PEER_VISIBILITY changed=1 value=0", timeout=10)
    for _ in range(2):
        dut.write("s")
        dut.expect_exact("INQUIRY_START started=1 running=1", timeout=10)
        completed = dut.expect(
            re.compile(rb"INQUIRY_COMPLETE cancelled=0 results=(\d+) dropped=0"),
            timeout=40,
            return_what_before_match=True,
        )
        assert peer_address.encode() not in completed

    # Visible again on request, so hiding is not a one-way door.
    peer.write("v\n")
    peer.expect_exact("INQUIRY_PEER_VISIBILITY changed=1 value=2", timeout=10)
    dut.write("s")
    dut.expect_exact("INQUIRY_START started=1 running=1", timeout=10)
    dut.expect(
        re.compile(rb"INQUIRY_RESULT address=" + peer_address.encode() + rb" "),
        timeout=40,
    )

    # --- asking a known address what it offers -----------------------------
    #
    # Inquiry answers "who is there"; these answer "what is it for" and "what is
    # it called". Both matter once a device publishes more than one service,
    # because an address alone does not say which service a sketch wants, and an
    # inquiry result can arrive with no name at all.
    dut.write("u")
    dut.write(peer_address + "\n")
    dut.expect_exact("INQUIRY_QUERY kind=u requested=1 error=None", timeout=10)
    services = dut.expect(
        re.compile(
            rb"INQUIRY_SERVICES peer=" + peer_address.encode() +
            rb" success=1 count=(\d+) reported=(\d+)([^\r\n]*)"
        ),
        timeout=30,
    )
    # The peer runs an SPP server, so its service list has to contain the SPP
    # UUID (0x1101). Anything else it publishes may come along too.
    assert int(services.group(1)) >= 1
    assert b"1101" in services.group(3).lower(), services.group(3)

    dut.write("n")
    dut.write(peer_address + "\n")
    dut.expect_exact("INQUIRY_QUERY kind=n requested=1 error=None", timeout=10)
    dut.expect_exact(
        "INQUIRY_NAME peer=" + peer_address + " success=1 name=EspBle Inquiry Peer",
        timeout=30,
    )

    # An address that cannot be parsed is refused before anything is sent.
    dut.write("U")
    dut.expect_exact(
        "INQUIRY_QUERY kind=U requested=0 error=InvalidArgument", timeout=10)
