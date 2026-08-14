import re


def connect_and_exchange(dut, peer, address):
    peer.write(b"c" + address + b"\n")
    peer.expect_exact("CLASSIC_HIDH_CONNECT_ACCEPTED 1", timeout=10)
    peer.expect(re.compile(rb"CLASSIC_HIDH_CONNECTED peer=[0-9a-f:]+"), timeout=30)
    # The device reports the connection from its event queue but sends the
    # input report from the polled connection state, so the two lines have no
    # fixed order. Both must appear; which one comes first is not a contract.
    announced = set()
    for _ in range(2):
        line = dut.expect(
            re.compile(rb"CLASSIC_HIDD_(CONNECTED) peer=[0-9a-f:]+|"
                       rb"CLASSIC_HIDD_(INPUT_ACCEPTED) 1"),
            timeout=30,
        )
        announced.add(line.group(1) or line.group(2))
    assert announced == {b"CONNECTED", b"INPUT_ACCEPTED"}
    incoming = peer.expect(
        re.compile(rb"CLASSIC_HIDH_INPUT id=(\d+) length=(\d+) hex=([0-9a-f]+)"),
        timeout=20,
    )
    assert incoming.group(3) in (b"007f80ff", b"01007f80ff")
    peer.expect_exact("CLASSIC_HIDH_OUTPUT_ACCEPTED 1", timeout=10)
    outgoing = dut.expect(
        re.compile(rb"CLASSIC_HIDD_OUTPUT id=(\d+) length=(\d+) hex=([0-9a-f]+)"),
        timeout=20,
    )
    assert outgoing.group(1) == b"2"
    assert outgoing.group(3) in (b"a500ff", b"02a500ff")


def test_classic_hid_device_to_host_report_and_output(dut, peers):
    peer = peers["device"]
    peer.expect_exact("CLASSIC_HIDH_READY", timeout=20)
    ready = dut.expect(
        re.compile(rb"CLASSIC_HIDD_READY address=([0-9a-f:]+)"), timeout=20
    )
    address = ready.group(1)
    connect_and_exchange(dut, peer, address)

    peer.write(b"s" + address + b"\n")
    peer.expect_exact("CLASSIC_COMPOSED_SPP_CONNECT 1", timeout=10)
    peer.expect(re.compile(rb"CLASSIC_COMPOSED_SPP_CONNECTED id=\d+"), timeout=30)
    dut.expect(re.compile(rb"CLASSIC_COMPOSED_SPP_CONNECTED id=\d+"), timeout=30)
    peer.expect_exact("CLASSIC_COMPOSED_SPP_WRITE 1", timeout=10)
    dut.expect_exact("CLASSIC_COMPOSED_SPP_RX length=5", timeout=10)
    peer.expect_exact("CLASSIC_COMPOSED_SPP_ECHO hex=00535050ff", timeout=10)

    dut.write("i")
    dut.expect_exact("CLASSIC_HIDD_INPUT_ACCEPTED 1", timeout=10)
    peer.expect(
        re.compile(rb"CLASSIC_HIDH_INPUT id=\d+ length=\d+ hex=(007f80ff|01007f80ff)"),
        timeout=20,
    )
    peer.expect_exact("CLASSIC_HIDH_OUTPUT_ACCEPTED 1", timeout=10)
    dut.expect(
        re.compile(rb"CLASSIC_HIDD_OUTPUT id=2 length=\d+ hex=(a500ff|02a500ff)"),
        timeout=20,
    )

    peer.write("d\n")
    peer.expect_exact("CLASSIC_HIDH_DISCONNECT_ACCEPTED 1", timeout=10)
    peer.expect_exact("CLASSIC_HIDH_DISCONNECTED", timeout=20)
    dut.expect_exact("CLASSIC_HIDD_DISCONNECTED", timeout=20)

    dut.write("r")
    dut.expect_exact("CLASSIC_HIDD_ENDED", timeout=20)
    restarted = dut.expect(
        re.compile(rb"CLASSIC_HIDD_READY address=([0-9a-f:]+)"), timeout=20
    )
    assert restarted.group(1) == address
    connect_and_exchange(dut, peer, address)
