import re


def test_hfp_sco_while_ble_gatt_remains_live(dut, peers, probe):
    peer = peers["device"]
    # Readiness is printed once at boot, before the monitor of the board that is
    # flashed second attaches. "?" repeats those lines on request.
    probe(dut, "?\n", re.compile(rb"DUAL_HFP_BLE_SERVER_READY"))
    client = dut.expect(
        re.compile(rb"HFP_CLIENT_READY address=([0-9a-f:]+)"), timeout=10
    )
    probe(peer, "?\n", re.compile(rb"DUAL_HFP_BLE_CLIENT_READY"))
    ag = peer.expect(
        re.compile(rb"HFP_AG_READY address=([0-9a-f:]+)"), timeout=10
    )
    assert client.group(1) != ag.group(1)

    # The BLE link forms on its own once both sides are up. Ask each side for its
    # state instead of waiting for the announcement it makes when it happens.
    probe(dut, "?\n", re.compile(rb"DUAL_HFP_BLE_SERVER_CONNECTED"))
    probe(peer, "r\n", re.compile(rb"DUAL_HFP_BLE_READ_REQUESTED 1"))
    peer.expect(re.compile(rb"DUAL_HFP_BLE_READ success=1 value=dual-hfp hfp=0"), timeout=20)

    dut.write(b"c" + ag.group(1) + b"\n")
    dut.expect_exact("HFP_CLIENT_CONNECT requested=1", timeout=10)
    dut.expect(re.compile(rb"HFP_CLIENT_CONNECTION state=3 peer=[0-9a-f:]+ features=\d+"), timeout=30)
    peer.expect(re.compile(rb"HFP_AG_CONNECTION state=3 peer=[0-9a-f:]+"), timeout=30)

    dut.write(b"d\n")
    dut.expect_exact("HFP_CLIENT_DIAL requested=1", timeout=10)
    peer.expect_exact("HFP_AG_DIAL number=12345", timeout=20)
    dut.expect(re.compile(rb"HFP_CLIENT_AUDIO state=2 codec=2 handle=\d+ frame=57"), timeout=30)
    peer.expect(re.compile(rb"HFP_AG_AUDIO state=2 codec=2 handle=\d+ frame=57"), timeout=30)

    dut.write(b"s\n")
    dut.expect_exact("HFP_CLIENT_SEND result=0", timeout=10)
    peer.expect(re.compile(rb"HFP_AG_MEDIA handle=\d+ len=58 bad=0 checksum=1653"), timeout=20)
    dut.expect(re.compile(rb"HFP_CLIENT_MEDIA codec=2 handle=\d+ len=60 bad=0 checksum=\d+"), timeout=20)

    peer.write(b"r\n")
    peer.expect_exact("DUAL_HFP_BLE_READ_REQUESTED 1", timeout=10)
    peer.expect(re.compile(rb"DUAL_HFP_BLE_READ success=1 value=dual-hfp hfp=1"), timeout=20)

    dut.write(b"z\n")
    diagnostics = dut.expect(
        re.compile(
            rb"DUAL_HFP_DIAGNOSTICS acl_tx=(\d+),(\d+) acl_rx=(\d+),(\d+) "
            rb"unknown=(\d+) mismatch=(\d+) qfull=(\d+)"
        ),
        timeout=10,
    )
    assert int(diagnostics.group(1)) > 0
    assert int(diagnostics.group(3)) > 0
    assert diagnostics.group(5) == b"0"
    assert diagnostics.group(6) == b"0"
    assert diagnostics.group(7) == b"0"

    dut.write(b"x\n")
    dut.expect_exact("HFP_CLIENT_AUDIO_DISCONNECT requested=1", timeout=10)
    dut.expect(re.compile(rb"HFP_CLIENT_AUDIO state=0 codec=0 handle=\d+ frame=\d+"), timeout=30)
    peer.expect(re.compile(rb"HFP_AG_AUDIO state=0 codec=0 handle=\d+ frame=\d+"), timeout=30)

    peer.write(b"r\n")
    peer.expect_exact("DUAL_HFP_BLE_READ_REQUESTED 1", timeout=10)
    peer.expect(re.compile(rb"DUAL_HFP_BLE_READ success=1 value=dual-hfp hfp=0"), timeout=20)
