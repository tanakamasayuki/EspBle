import re


def test_a2dp_sink_receives_external_codec_media(dut, peers):
    peer = peers["device"]
    dut.expect_exact("AVRCP_SINK_READY", timeout=30)
    ready = dut.expect(
        re.compile(
            rb"A2DP_SINK_READY started=1 address=([0-9a-f:]+) error=None:"
        ),
        timeout=30,
    )
    peer.expect_exact("AVRCP_SOURCE_READY", timeout=30)
    peer.expect_exact("A2DP_SOURCE_PROFILE initialized=1", timeout=30)
    peer.expect_exact("A2DP_SOURCE_READY endpoint=1 seid=0", timeout=30)
    peer.write(b"c" + ready.group(1) + b"\n")
    peer.expect_exact("A2DP_SOURCE_CONNECT requested=1", timeout=10)
    peer.expect(
        [
            re.compile(rb"A2DP_SOURCE_CONNECTED id=\d+ mtu=\d+"),
            re.compile(
                rb"AVRCP_SOURCE_CONNECTION controller=1 connected=1 peer=[0-9a-f:]+"
            ),
        ],
        timeout=30,
        expect_all=True,
    )
    connected_pattern = re.compile(
        rb"A2DP_SINK_CONNECTED id=\d+ peer=[0-9a-f:]+ mtu=\d+ incoming=1"
    )
    codec_pattern = re.compile(
        rb"A2DP_SINK_CODEC id=\d+ codec=1 rate=(\d+) channels=(\d+) "
        rb"mode=\d+ blocks=\d+ subbands=\d+ alloc=\d+ bitpool=\d+-\d+ raw_len=4"
    )
    # Bluedroid may publish the selected codec just before or just after the
    # connection event. Preserve both instead of making callback order an API.
    first = dut.expect([connected_pattern, codec_pattern], timeout=30)
    if first.re is connected_pattern:
        codec = dut.expect(codec_pattern, timeout=30)
    else:
        codec = first
        dut.expect(connected_pattern, timeout=30)
    assert codec.group(1) == b"48000"
    assert codec.group(2) == b"2"
    peer.write(b"v\n")
    peer.expect_exact("AVRCP_SOURCE_REGISTER_VOLUME requested=1", timeout=10)
    peer.expect_exact("AVRCP_SOURCE_PLAY requested=1", timeout=10)
    peer.expect_exact("AVRCP_SOURCE_SET_VOLUME requested=1", timeout=10)
    peer.expect_exact("A2DP_SOURCE_START requested=1", timeout=10)
    dut.expect(
        [
            re.compile(rb"AVRCP_SINK_KEY command=68 state=0"),
            re.compile(rb"AVRCP_SINK_KEY command=68 state=1"),
            re.compile(rb"AVRCP_SINK_VOLUME value=77 remote=1"),
            re.compile(rb"AVRCP_SINK_LOCAL_VOLUME changed=1"),
            re.compile(rb"A2DP_SINK_STREAM id=\d+ state=1"),
        ],
        timeout=20,
        expect_all=True,
    )
    peer.expect(
        [
            re.compile(rb"AVRCP_SOURCE_KEY_RESPONSE command=68 state=0 accepted=1"),
            re.compile(rb"AVRCP_SOURCE_KEY_RESPONSE command=68 state=1 accepted=1"),
            re.compile(rb"AVRCP_SOURCE_VOLUME value=77 remote=0"),
            re.compile(rb"AVRCP_SOURCE_VOLUME value=88 remote=0"),
        ],
        timeout=20,
        expect_all=True,
    )
    media = dut.expect(
        re.compile(
            rb"A2DP_SINK_MEDIA id=\d+ codec=1 timestamp=(\d+) frames=1 "
            rb"len=13 checksum=\d+ first=9c"
        ),
        timeout=30,
    )
    assert int(media.group(1)) >= 1000
    disconnected = dut.expect(
        re.compile(rb"A2DP_SINK_DISCONNECTED id=\d+ packets=(\d+) bytes=(\d+)"),
        timeout=30,
    )
    assert disconnected.group(1) == b"100"
    assert disconnected.group(2) == b"1300"
    dut.expect_exact("A2DP_SINK_MEDIA_UNREGISTERED", timeout=10)
    dut.expect_exact("A2DP_SINK_ENDED initialized=0", timeout=10)
    source_disconnected = peer.expect(
        re.compile(rb"A2DP_SOURCE_DISCONNECTED sent=100 would_block=(\d+)"),
        timeout=30,
    )
    assert int(source_disconnected.group(1)) > 0
