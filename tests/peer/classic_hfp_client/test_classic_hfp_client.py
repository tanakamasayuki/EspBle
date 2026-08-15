import re


def test_hfp_client_control_and_external_codec_audio(dut, peers, probe):
    peer = peers["device"]
    # Readiness is printed once at boot, before the monitor of the board that is
    # flashed second attaches. "?" repeats those lines on request.
    probe(
        dut, "?\n", re.compile(rb"HFP_CLIENT_EXCLUSION ag=0 error=InvalidState")
    )
    client = dut.expect(
        re.compile(rb"HFP_CLIENT_READY address=([0-9a-f:]+)"), timeout=10
    )
    probe(
        peer, "?\n", re.compile(rb"HFP_AG_EXCLUSION client=0 error=InvalidState")
    )
    ag = peer.expect(
        re.compile(rb"HFP_AG_READY address=([0-9a-f:]+)"), timeout=10
    )
    assert client.group(1) != ag.group(1)

    dut.write(b"c" + ag.group(1) + b"\n")
    dut.expect_exact("HFP_CLIENT_CONNECT requested=1", timeout=10)
    dut.expect(re.compile(rb"HFP_CLIENT_CONNECTION state=3 peer=[0-9a-f:]+ features=\d+"), timeout=30)
    peer.expect(re.compile(rb"HFP_AG_CONNECTION state=3 peer=[0-9a-f:]+"), timeout=30)

    # What the phone knows about itself, which only arrives when asked. The AG
    # answers +COPS and +CNUM from its own configuration, so the values here are
    # the ones the peer sketch set: service type 4 is voice.
    dut.write(b"o\n")
    dut.expect_exact("HFP_CLIENT_OPERATOR_REQUEST requested=1", timeout=10)
    dut.expect_exact("HFP_CLIENT_OPERATOR name=EspBle", timeout=20)
    dut.write(b"u\n")
    dut.expect_exact("HFP_CLIENT_SUBSCRIBER_REQUEST requested=1", timeout=10)
    dut.expect_exact("HFP_CLIENT_SUBSCRIBER number=5550000 type=4", timeout=20)

    # A memory dial is a position, not a number, and the AG has to be able to
    # tell the two apart or it would dial "3".
    dut.write(b"m\n")
    dut.expect_exact("HFP_CLIENT_DIAL_MEMORY requested=1", timeout=10)
    peer.expect_exact("HFP_AG_DIAL_MEMORY location=3", timeout=20)
    peer.expect_exact("HFP_AG_DIAL_MEMORY_RESPONSE accepted=1", timeout=10)
    dut.write(b"M\n")
    dut.expect_exact(
        "HFP_CLIENT_DIAL_MEMORY_INVALID requested=0 error=InvalidArgument",
        timeout=10,
    )

    # Asking the phone to stop its own noise reduction reaches the AG as a
    # command with the state it asked for: AT+NREC=0 means off.
    dut.write(b"e\n")
    dut.expect_exact("HFP_CLIENT_NREC requested=1", timeout=10)
    peer.expect_exact("HFP_AG_NREC enabled=0", timeout=20)

    # The Apple extensions have no decoder in the AG API, so they arrive as
    # unknown AT text — which is exactly how a phone that does understand them
    # tells a battery level from a Siri status.
    dut.write(b"A\n")
    dut.expect_exact("HFP_CLIENT_XAPL requested=1", timeout=10)
    peer.expect(re.compile(rb"HFP_AG_UNAT value=[^\r\n]*XAPL[^\r\n]*"), timeout=20)
    peer.expect_exact("HFP_AG_UNAT_RESPONSE accepted=1 apple=1", timeout=10)
    dut.write(b"B\n")
    dut.expect_exact("HFP_CLIENT_BATTERY requested=1", timeout=10)
    peer.expect(
        re.compile(rb"HFP_AG_UNAT value=[^\r\n]*IPHONEACCEV[^\r\n]*"), timeout=20
    )

    # Both bad arguments are refused locally: an empty identification would send
    # a malformed XAPL, and Apple's battery level only goes up to 9.
    dut.write(b"J\n")
    dut.expect_exact(
        "HFP_CLIENT_XAPL_INVALID requested=0 error=InvalidArgument", timeout=10
    )
    dut.write(b"K\n")
    dut.expect_exact(
        "HFP_CLIENT_BATTERY_INVALID requested=0 error=InvalidArgument", timeout=10
    )

    # Whether the phone sends the ring tone itself. An accessory that beeps on
    # its own has to be told, and the answer can change between calls.
    peer.write(b"b\n")
    peer.expect_exact("HFP_AG_INBAND set=1 provided=1", timeout=10)
    dut.expect_exact("HFP_CLIENT_INBAND provided=1", timeout=20)
    peer.write(b"B\n")
    peer.expect_exact("HFP_AG_INBAND set=1 provided=0", timeout=10)
    dut.expect_exact("HFP_CLIENT_INBAND provided=0", timeout=20)

    # A request this AG cannot satisfy: the phone recorded no voice tag, so what
    # matters is that the link survives the refusal and still carries a call.
    dut.write(b"V\n")
    dut.expect_exact("HFP_CLIENT_VOICETAG_REQUEST requested=1", timeout=10)

    dut.write(b"d\n")
    dut.expect_exact("HFP_CLIENT_DIAL requested=1", timeout=10)
    peer.expect_exact("HFP_AG_DIAL number=12345", timeout=20)
    dut.expect(re.compile(rb"HFP_CLIENT_CALL active=1 setup=0 held=\d+"), timeout=20)

    # The AG probe transitions the call directly to active, which causes the
    # HFP stack to establish SCO without a second explicit connect request.
    client_audio = dut.expect(
        re.compile(rb"HFP_CLIENT_AUDIO state=2 codec=([23]) handle=(\d+) frame=(\d+)"),
        timeout=30,
    )
    peer.expect(
        re.compile(rb"HFP_AG_AUDIO state=2 codec=([23]) handle=\d+ frame=\d+"),
        timeout=30,
    )
    assert int(client_audio.group(3)) > 0

    dut.write(b"s\n")
    dut.expect_exact("HFP_CLIENT_SEND result=0", timeout=10)
    peer.expect(re.compile(rb"HFP_AG_MEDIA handle=\d+ len=58 bad=0 checksum=1653"), timeout=20)
    echoed = dut.expect(
        re.compile(rb"HFP_CLIENT_MEDIA codec=2 handle=\d+ len=60 bad=0 checksum=(\d+)"),
        timeout=20,
    )
    assert int(echoed.group(1)) > 0

    dut.write(b"p\n")
    dut.expect_exact("HFP_CLIENT_STATS_REQUEST requested=1", timeout=10)
    dut.expect(re.compile(rb"HFP_CLIENT_STATS rx=\d+ ok=\d+ bad=\d+ tx=\d+ drop=\d+"), timeout=20)

    dut.write(b"x\n")
    dut.expect_exact("HFP_CLIENT_AUDIO_DISCONNECT requested=1", timeout=10)
    dut.expect(re.compile(rb"HFP_CLIENT_AUDIO state=0 codec=0 handle=\d+ frame=\d+"), timeout=30)

    dut.write(b"h\n")
    dut.expect_exact("HFP_CLIENT_HANGUP requested=1", timeout=10)
    peer.expect_exact("HFP_AG_HANGUP ended=1", timeout=20)
    dut.expect(re.compile(rb"HFP_CLIENT_CALL active=0 setup=0 held=\d+"), timeout=20)

    peer.write(b"i\n")
    peer.expect_exact("HFP_AG_INCOMING reported=1", timeout=10)
    dut.expect(re.compile(rb"HFP_CLIENT_CALL active=0 setup=1 held=\d+"), timeout=20)

    dut.write(b"n\n")
    dut.expect_exact("HFP_CLIENT_ANSWER requested=1", timeout=10)
    peer.expect_exact("HFP_AG_ANSWER active=1", timeout=20)
    dut.expect(re.compile(rb"HFP_CLIENT_CALL active=1 setup=0 held=\d+"), timeout=20)

    dut.write(b"h\n")
    dut.expect_exact("HFP_CLIENT_HANGUP requested=1", timeout=10)
    peer.expect_exact("HFP_AG_HANGUP ended=1", timeout=20)
    dut.expect(re.compile(rb"HFP_CLIENT_CALL active=0 setup=0 held=\d+"), timeout=20)
