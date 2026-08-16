import re


def test_hfp_interoperates_with_the_core_bluedroid_audio_gateway(dut, peers, probe):
    """EspBle's HFP Client against the Bluedroid Audio Gateway the core ships.

    The peer links no EspBle code: its AG comes from `esp_hf_ag_*`. The
    service-level connection, the call indicators and the call-control AT
    exchanges therefore cross a stack boundary.

    SCO audio is not part of this suite. The core's HFP is built with
    `CONFIG_BT_HFP_AUDIO_DATA_PATH_PCM`, so the AG routes voice to an external
    codec chip and an application on that board never sees the payload. EspBle's
    raw SCO transport is covered against an EspBle AG in `classic_hfp_client`.
    """
    peer = peers["device"]

    ready = probe(peer, "?\n", re.compile(rb"HFPPEER_READY address=([0-9a-f:]+)"))
    ag_address = ready.group(1)
    probe(dut, "?\n", re.compile(rb"HFPCLIENT_READY started=1 address=[0-9a-f:]+"))

    # The Client initiates, which means pairing and the whole SLC negotiation —
    # feature exchange, indicator discovery, indicator activation — run between
    # the two independently built stacks.
    dut.write(b"c" + ag_address + b"\n")
    dut.expect(re.compile(rb"HFPCLIENT_CONNECT requested=1"), timeout=10)
    peer.expect(re.compile(rb"HFPPEER_CONNECTION state=2 peer=[0-9a-f:]+"), timeout=60)
    connection = dut.expect(
        re.compile(rb"HFPCLIENT_CONNECTION state=3 peer=[0-9a-f:]+ features=(\d+)"),
        timeout=60,
    )
    assert int(connection.group(1)) > 0, "the AG advertised no features"

    # An incoming call on the AG has to reach the Client as the call-setup
    # indicator plus the caller identity, both produced by the other stack.
    peer.write("i\n")
    peer.expect(re.compile(rb"HFPPEER_INCOMING requested=0"), timeout=10)
    dut.expect(re.compile(rb"HFPCLIENT_CALL active=0 setup=1 held=\d+"), timeout=30)

    # Answering travels the other way as ATA, and the AG reports it received it.
    dut.write("a\n")
    dut.expect_exact("HFPCLIENT_ANSWER requested=1", timeout=10)
    peer.expect(re.compile(rb"HFPPEER_ANSWER count=1"), timeout=30)
    dut.expect(re.compile(rb"HFPCLIENT_CALL active=1 setup=0 held=\d+"), timeout=30)

    # Hanging up is CHUP, and the call indicators must go back to idle on the
    # Client side once the AG reports the change.
    dut.write("h\n")
    dut.expect_exact("HFPCLIENT_HANGUP requested=1", timeout=10)
    peer.expect(re.compile(rb"HFPPEER_HANGUP count=1"), timeout=30)
    dut.expect(re.compile(rb"HFPCLIENT_CALL active=0 setup=0 held=\d+"), timeout=30)

    # Dialling sends ATD with the number; the AG prints what it actually parsed,
    # so a Client that mangles the AT line fails here rather than silently.
    dut.write("d\n")
    dut.expect_exact("HFPCLIENT_DIAL requested=1", timeout=10)
    peer.expect_exact("HFPPEER_DIAL number=12345", timeout=30)
    dut.expect(re.compile(rb"HFPCLIENT_CALL active=\d setup=2 held=\d+"), timeout=30)

    peer.write("e\n")
    peer.expect(re.compile(rb"HFPPEER_END requested=0"), timeout=10)
    dut.expect(re.compile(rb"HFPCLIENT_CALL active=0 setup=0 held=\d+"), timeout=30)

    dut.write("q\n")
    dut.expect_exact("HFPCLIENT_DISCONNECT requested=1", timeout=10)
    dut.expect(re.compile(rb"HFPCLIENT_CONNECTION state=0 peer=[0-9a-f:]+"), timeout=30)
    peer.expect(re.compile(rb"HFPPEER_CONNECTION state=0 peer=[0-9a-f:]+"), timeout=30)
