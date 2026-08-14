import re


PAYLOAD_HEX = b"007f80ff535050"


def connect_and_echo(dut, peer, address):
    peer.write(f"c{address}\n")
    peer.expect_exact("CLASSIC_PEER_CONNECT_ACCEPTED 1", timeout=10)
    peer.expect(re.compile(rb"CLASSIC_PEER_CONNECTED id=\d+ incoming=0"), timeout=20)
    dut.expect(re.compile(rb"CLASSIC_CONNECTED id=\d+ incoming=1"), timeout=20)
    peer.expect_exact("CLASSIC_PEER_WRITE_ACCEPTED 1", timeout=10)
    dut.expect(re.compile(rb"CLASSIC_RX id=\d+ length=7 hex=" + PAYLOAD_HEX), timeout=10)
    dut.expect_exact("CLASSIC_ECHO_ACCEPTED 1", timeout=10)
    peer.expect(re.compile(rb"CLASSIC_PEER_ECHO id=\d+ length=7 hex=" + PAYLOAD_HEX), timeout=10)


def test_classic_spp_server_client_and_restart(dut, peers):
    peer = peers["device"]
    peer.expect_exact("CLASSIC_PEER_READY", timeout=20)
    ready = dut.expect(
        re.compile(rb"CLASSIC_SERVER_READY address=([0-9a-f:]+) heap=(\d+)"),
        timeout=20,
    )
    address = ready.group(1).decode()
    initial_heap = int(ready.group(2))
    dut.expect_exact("CLASSIC_SERVER_STARTED", timeout=10)

    connect_and_echo(dut, peer, address)
    peer.write("d\n")
    peer.expect_exact("CLASSIC_PEER_DISCONNECT_ACCEPTED 1", timeout=10)
    peer.expect(re.compile(rb"CLASSIC_PEER_DISCONNECTED id=\d+"), timeout=10)
    dut.expect(re.compile(rb"CLASSIC_DISCONNECTED id=\d+"), timeout=10)

    dut.write("r")
    ended = dut.expect(re.compile(rb"CLASSIC_ENDED heap=(\d+)"), timeout=20)
    restarted = dut.expect(
        re.compile(rb"CLASSIC_SERVER_READY address=([0-9a-f:]+) heap=(\d+)"),
        timeout=20,
    )
    dut.expect_exact("CLASSIC_SERVER_STARTED", timeout=10)
    assert restarted.group(1).decode() == address
    assert int(ended.group(1)) > 0
    assert int(restarted.group(2)) >= initial_heap - 8192

    connect_and_echo(dut, peer, address)

    # --- several services on one device ------------------------------------
    #
    # A device that offers more than one serial service needs one record per
    # service, each on its own channel. Publishing a second one must not replace
    # the first, and a peer has to be able to reach the one it wants — discovery
    # returns every channel, so the choice cannot come from there.
    dut.write("?")
    dut.expect_exact("CLASSIC_SERVERS count=1", timeout=10)
    dut.write("2")
    dut.expect_exact("CLASSIC_SECOND_SERVER started=1 error=None", timeout=10)
    second = dut.expect(
        re.compile(rb"CLASSIC_SERVER_STARTED channel=(\d+)"), timeout=20)
    second_channel = int(second.group(1))
    dut.write("?")
    listed = dut.expect(re.compile(rb"CLASSIC_SERVERS count=2 [^\r\n]*"), timeout=10)
    channels = re.findall(rb"(\d+):", listed.group(0))
    assert len(set(channels)) == 2, channels

    # The second service accepts a connection of its own, which is what proves
    # the record is real rather than only counted locally. The client side holds
    # one session at a time, so the first one is closed before dialling again.
    peer.write("d\n")
    peer.expect_exact("CLASSIC_PEER_DISCONNECT_ACCEPTED 1", timeout=10)
    peer.expect(re.compile(rb"CLASSIC_PEER_DISCONNECTED id=\d+"), timeout=20)
    dut.expect(re.compile(rb"CLASSIC_DISCONNECTED id=\d+"), timeout=20)
    peer.write(f"k{address}:{second_channel}\n")
    peer.expect_exact("CLASSIC_PEER_CONNECT_ACCEPTED 1", timeout=10)
    peer.expect(re.compile(rb"CLASSIC_PEER_CONNECTED id=\d+ incoming=0"), timeout=30)
    dut.expect(re.compile(rb"CLASSIC_CONNECTED id=\d+ incoming=1"), timeout=20)
    peer.expect_exact("CLASSIC_PEER_WRITE_ACCEPTED 1", timeout=10)
    dut.expect(
        re.compile(rb"CLASSIC_RX id=\d+ length=7 hex=" + PAYLOAD_HEX), timeout=10)
    peer.write("d\n")
    peer.expect_exact("CLASSIC_PEER_DISCONNECT_ACCEPTED 1", timeout=10)
    dut.expect(re.compile(rb"CLASSIC_DISCONNECTED id=\d+"), timeout=20)

    # stopServer() takes down every service this object started; there is no
    # per-channel stop, so this is what a sketch uses to retire one.
    dut.write("s")
    dut.expect_exact("CLASSIC_SERVERS_STOPPED 1", timeout=10)
    dut.write("?")
    dut.expect_exact("CLASSIC_SERVERS count=0", timeout=10)

    # Starting again after a full stop republishes the service, so stopping is
    # not a one-way door.
    dut.write("1")
    dut.expect_exact("CLASSIC_FIRST_SERVER started=1 error=None", timeout=10)
    dut.expect(re.compile(rb"CLASSIC_SERVER_STARTED channel=\d+"), timeout=20)
    dut.write("?")
    dut.expect_exact("CLASSIC_SERVERS count=1", timeout=10)
    connect_and_echo(dut, peer, address)
