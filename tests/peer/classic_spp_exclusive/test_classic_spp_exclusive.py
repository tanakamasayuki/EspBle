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
