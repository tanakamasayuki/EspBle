import re


def test_hfp_client_control_and_external_codec_audio(dut, peers):
    peer = peers["device"]
    client = dut.expect(
        re.compile(rb"HFP_CLIENT_READY address=([0-9a-f:]+)"), timeout=30
    )
    ag = peer.expect(
        re.compile(rb"HFP_AG_READY address=([0-9a-f:]+)"), timeout=30
    )
    assert client.group(1) != ag.group(1)

    dut.write(b"c" + ag.group(1) + b"\n")
    dut.expect_exact("HFP_CLIENT_CONNECT requested=1", timeout=10)
    dut.expect(re.compile(rb"HFP_CLIENT_CONNECTION state=3 peer=[0-9a-f:]+ features=\d+"), timeout=30)
    peer.expect(re.compile(rb"HFP_AG_CONNECTION state=3 peer=[0-9a-f:]+"), timeout=30)

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
    peer.expect(re.compile(rb"HFP_AG_AUDIO state=[23] handle=\d+ frame=\d+"), timeout=30)
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
