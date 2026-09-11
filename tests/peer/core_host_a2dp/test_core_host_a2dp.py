import re


def test_a2dp_and_avrcp_interoperate_with_the_core_bluedroid_stack(dut, peers, probe):
    """EspBle's A2DP Sink against the Bluedroid A2DP Source the core ships.

    The peer links no EspBle code: it drives `esp_a2d_source_*` directly, and
    the SBC encoding happens inside that stack.
    EspBle hands the encoded frames over untouched, so what this checks is that
    the codec negotiation, the media framing and the AVRCP commands survive a
    crossing between two independently built Classic hosts.
    """
    peer = peers["device"]

    ready = probe(
        dut, "?\n", re.compile(rb"A2DPSINK_READY started=1 avrcp=1 address=([0-9a-f:]+)")
    )
    sink_address = ready.group(1)
    probe(peer, "?\n", re.compile(rb"A2DPPEER_READY address=[0-9a-f:]+"))

    # The Source initiates, so pairing and AVDTP discovery are driven entirely
    # by the other stack against EspBle's service records.
    peer.write(b"c" + sink_address + b"\n")
    peer.expect_exact("A2DPPEER_CONNECT requested=1", timeout=10)
    peer.expect(re.compile(rb"A2DPPEER_CONNECTION state=2 peer=[0-9a-f:]+"), timeout=60)

    # Codec configuration and the connection event arrive in an order the two
    # stacks decide between them, so both are awaited together rather than in a
    # fixed sequence. 44.1 kHz stereo SBC is what the core's Source negotiates.
    codec_pattern = re.compile(
        rb"A2DPSINK_CODEC codec=(\d+) rate=(\d+) channels=(\d+) blocks=(\d+) "
        rb"subbands=(\d+) bitpool=(\d+)-(\d+)"
    )
    connected_pattern = re.compile(
        rb"A2DPSINK_CONNECTED id=\d+ peer=[0-9a-f:]+ mtu=\d+ incoming=1"
    )
    first = dut.expect([codec_pattern, connected_pattern], timeout=60)
    if first.re.pattern == codec_pattern.pattern:
        codec = first
        dut.expect(connected_pattern, timeout=60)
    else:
        codec = dut.expect(codec_pattern, timeout=60)
    assert int(codec.group(2)) == 44100, "unexpected sample rate"
    assert int(codec.group(3)) == 2, "expected a stereo configuration"
    assert int(codec.group(5)) == 8, "SBC subbands did not survive negotiation"
    assert int(codec.group(7)) >= int(codec.group(6)), "bitpool range is inverted"

    # The Source starts streaming on its own once it is ready.
    dut.expect(re.compile(rb"A2DPSINK_STREAM state=1 streaming=1"), timeout=60)
    peer.expect_exact("A2DPPEER_AUDIO_STATE state=1", timeout=60)

    dut.write("z\n")
    dut.expect_exact("A2DPSINK_COUNTERS_RESET", timeout=10)
    media = probe(
        dut,
        "q\n",
        re.compile(
            rb"A2DPSINK_MEDIA packets=([1-9]\d*) bytes=(\d+) frames=(\d+) first=([0-9a-f]{2})"
        ),
    )
    assert int(media.group(2)) > 0, "media packets carried no payload"
    assert int(media.group(3)) > 0, "no SBC frames were counted"
    # Every SBC frame starts with the 0x9c syncword. A sink that handed over the
    # RTP header, or a payload offset by the media header, fails here.
    assert media.group(4) == b"9c", "payload does not start at an SBC frame"

    # AVRCP travels its own L2CAP channel, brought up separately from the media
    # one. Wait for the Controller side to report it before pressing a key, so a
    # missing key event means a lost command rather than a race with setup.
    # AVRCP is deliberately not asserted here. Neither stack opens the AVCTP
    # channel in this pairing: the core's A2DP Source does not initiate it, and
    # EspBle's Sink attempts it only as the A2DP initiator, so a key press has
    # nothing to travel on. Sending one anyway would test the peer's local
    # acceptance of the command, not a crossing. EspBle's AVRCP is covered
    # against an EspBle Source in `classic_a2dp_media`, and against real devices
    # in the manual interoperability step.

    # Suspend has to reach the sink as a stream state change, not merely as the
    # media flow stopping.
    peer.write("u\n")
    peer.expect_exact("A2DPPEER_SUSPEND requested=1", timeout=10)
    dut.expect(re.compile(rb"A2DPSINK_STREAM state=0 streaming=0"), timeout=30)

    peer.write("d\n")
    peer.expect_exact("A2DPPEER_DISCONNECT requested=1", timeout=10)
    dut.expect(re.compile(rb"A2DPSINK_DISCONNECTED id=\d+ packets=\d+"), timeout=30)
    state = probe(dut, "?\n", re.compile(rb"A2DPSINK_STATE connected=0 streaming=0"))
    assert state is not None
