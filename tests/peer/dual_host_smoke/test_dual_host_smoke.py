import re


def test_nimble_and_custom_classic_host_run_together(dut, peers):
    peer = peers["device"]
    peer.expect_exact("DUAL_PEER_READY", timeout=20)
    ready = dut.expect(
        re.compile(
            rb"DUAL_READY classic=([0-9a-f:]+) ble=([0-9a-f:]+) type=(\d+)"
        ),
        timeout=30,
    )

    assert ready.group(3) == b"0"
    peer.write(b"c" + ready.group(1) + b"\n")
    peer.expect_exact("DUAL_PEER_CONNECT 1", timeout=10)
    dut.expect_exact("DUAL_CLASSIC_CONNECTED", timeout=30)
    peer.expect_exact("DUAL_PEER_CONNECTED", timeout=30)
    dut.write("i")
    dut.expect_exact("DUAL_CLASSIC_INPUT 1", timeout=10)
    peer.expect(re.compile(rb"DUAL_PEER_INPUT hex=(01)?007f80[0-9a-f]{2}"), timeout=20)
    peer.expect_exact("DUAL_PEER_OUTPUT 1", timeout=10)
    dut.expect(re.compile(rb"DUAL_CLASSIC_OUTPUT id=2 hex=(a500ff|02a500ff)"), timeout=20)

    peer.write(b"b" + ready.group(2) + b"\n")
    peer.expect_exact("DUAL_BLE_CONNECT 1", timeout=20)
    peer.expect_exact("DUAL_BLE_READ_REQUESTED 1", timeout=20)
    peer.expect_exact("DUAL_BLE_READ success=1 value=dual-ready classic=1", timeout=20)

    # Cross the controller's former 20-packet host-buffer limit by a wide
    # margin.  One initial read plus these repeats generates over 50 LE ACL
    # packets across the two directions.
    for _ in range(24):
        peer.write("r\n")
        peer.expect_exact("DUAL_BLE_READ_REQUESTED 1", timeout=10)
        peer.expect_exact(
            "DUAL_BLE_READ success=1 value=dual-ready classic=1", timeout=10
        )

    peer.write("o\n")
    peer.expect_exact("DUAL_PEER_OUTPUT 1", timeout=10)
    dut.expect(re.compile(rb"DUAL_CLASSIC_OUTPUT id=2 hex=(a500ff|02a500ff)"), timeout=10)

    dut.write("i")
    dut.expect_exact("DUAL_CLASSIC_INPUT 1", timeout=10)
    peer.expect(re.compile(rb"DUAL_PEER_INPUT hex=(01)?007f80[0-9a-f]{2}"), timeout=10)
    peer.expect_exact("DUAL_PEER_OUTPUT 1", timeout=10)
    dut.expect(re.compile(rb"DUAL_CLASSIC_OUTPUT id=2 hex=(a500ff|02a500ff)"), timeout=10)

    dut.write("d")
    peer.write("d\n")
    dut_diag = dut.expect(re.compile(rb"DUAL_DIAG .*"), timeout=10)
    peer_diag = peer.expect(re.compile(rb"DUAL_PEER_DIAG .*"), timeout=10)
    print(dut_diag.group(0).decode())
    print(peer_diag.group(0).decode())

    dut.write("?")
    dut.expect_exact("DUAL_STATE adv=0 classic=1 ble=1", timeout=10)
