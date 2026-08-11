import re


def test_hfp_cvsd_audio_disconnect_and_reconnect(dut, peers):
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
    dut.expect(
        re.compile(rb"HFP_CLIENT_CONNECTION state=3 peer=[0-9a-f:]+"),
        timeout=30,
    )
    peer.expect(
        re.compile(rb"HFP_AG_CONNECTION state=3 peer=[0-9a-f:]+"),
        timeout=30,
    )

    dut.write(b"d\n")
    dut.expect_exact("HFP_CLIENT_DIAL requested=1", timeout=10)
    peer.expect_exact("HFP_AG_DIAL number=12345", timeout=20)
    dut.expect(
        re.compile(rb"HFP_CLIENT_CALL active=1 setup=0 held=\d+"), timeout=20
    )

    def expect_cvsd_audio():
        client_audio = dut.expect(
            re.compile(rb"HFP_CLIENT_AUDIO state=2 codec=3 handle=\d+ frame=(\d+)"),
            timeout=30,
        )
        ag_audio = peer.expect(
            re.compile(rb"HFP_AG_AUDIO state=2 codec=3 handle=\d+ frame=(\d+)"),
            timeout=30,
        )
        assert int(client_audio.group(1)) > 0
        assert int(ag_audio.group(1)) > 0

    def exchange_cvsd_audio():
        dut.write(b"s\n")
        dut.expect_exact("HFP_CLIENT_SEND result=0", timeout=10)
        peer.expect(
            re.compile(
                rb"HFP_AG_MEDIA handle=\d+ len=\d+ bad=0 "
                rb"checksum=\d+ codec=3"
            ),
            timeout=20,
        )
        peer.expect_exact("HFP_AG_SEND result=0", timeout=10)
        dut.expect(
            re.compile(
                rb"HFP_CLIENT_MEDIA codec=3 handle=\d+ len=\d+ "
                rb"bad=0 checksum=\d+"
            ),
            timeout=20,
        )

    def assert_packet_statistics():
        dut.write(b"p\n")
        dut.expect_exact("HFP_CLIENT_STATS_REQUEST requested=1", timeout=10)
        statistics = dut.expect(
            re.compile(
                rb"HFP_CLIENT_STATS rx=(\d+) ok=(\d+) bad=(\d+) "
                rb"tx=(\d+) drop=(\d+)"
            ),
            timeout=20,
        )
        assert int(statistics.group(1)) > 0
        assert int(statistics.group(2)) > 0
        assert int(statistics.group(4)) > 0
        assert statistics.group(5) == b"0"

    expect_cvsd_audio()
    exchange_cvsd_audio()
    assert_packet_statistics()

    dut.write(b"x\n")
    dut.expect_exact("HFP_CLIENT_AUDIO_DISCONNECT requested=1", timeout=10)
    dut.expect(
        re.compile(rb"HFP_CLIENT_AUDIO state=0 codec=0 handle=\d+ frame=\d+"),
        timeout=30,
    )
    peer.expect(
        re.compile(rb"HFP_AG_AUDIO state=0 codec=0 handle=\d+ frame=\d+"),
        timeout=30,
    )

    dut.write(b"a\n")
    dut.expect_exact("HFP_CLIENT_AUDIO_CONNECT requested=1", timeout=10)
    expect_cvsd_audio()
    exchange_cvsd_audio()
    assert_packet_statistics()

    dut.write(b"x\n")
    dut.expect_exact("HFP_CLIENT_AUDIO_DISCONNECT requested=1", timeout=10)
    dut.expect(
        re.compile(rb"HFP_CLIENT_AUDIO state=0 codec=0 handle=\d+ frame=\d+"),
        timeout=30,
    )
    peer.expect(
        re.compile(rb"HFP_AG_AUDIO state=0 codec=0 handle=\d+ frame=\d+"),
        timeout=30,
    )

    dut.write(b"h\n")
    dut.expect_exact("HFP_CLIENT_HANGUP requested=1", timeout=10)
    peer.expect_exact("HFP_AG_HANGUP ended=1", timeout=20)
    dut.expect(
        re.compile(rb"HFP_CLIENT_CALL active=0 setup=0 held=\d+"), timeout=20
    )
