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
    for diag in (dut_diag.group(0), peer_diag.group(0)):
        command = re.search(
            rb"cmd=(\d+),(\d+)/(\d+),(\d+) qmax=(\d+) "
            rb"qfull=(\d+) mismatch=(\d+) busy=(\d+)",
            diag,
        )
        assert command is not None
        assert int(command.group(1)) > 0
        assert int(command.group(2)) > 0
        assert command.group(1) == command.group(3)
        assert command.group(2) == command.group(4)
        assert int(command.group(5)) >= 1
        assert command.group(6) == b"0"
        assert command.group(7) == b"0"
        assert command.group(8) == b"0"

    dut.write("?")
    dut.expect_exact("DUAL_STATE adv=0 classic=1 ble=1", timeout=10)

    # Classic owns the BTDM controller.  Reverse shutdown must be rejected
    # before either host or profile is touched.
    peer.write("x\n")
    peer.expect_exact(
        "DUAL_PEER_REVERSE ble=1 classic=1 error=InvalidState", timeout=10
    )
    dut.write("x")
    dut.expect_exact("DUAL_REVERSE ble=1 classic=1 error=InvalidState", timeout=10)

    peer.write("r\n")
    peer.expect_exact("DUAL_BLE_READ_REQUESTED 1", timeout=10)
    peer.expect_exact(
        "DUAL_BLE_READ success=1 value=dual-ready classic=1", timeout=10
    )
    dut.write("i")
    dut.expect_exact("DUAL_CLASSIC_INPUT 1", timeout=10)
    peer.expect(re.compile(rb"DUAL_PEER_INPUT hex=(01)?007f80[0-9a-f]{2}"), timeout=10)
    peer.expect_exact("DUAL_PEER_OUTPUT 1", timeout=10)
    dut.expect(
        re.compile(rb"DUAL_CLASSIC_OUTPUT id=2 hex=(a500ff|02a500ff)"),
        timeout=10,
    )

    # The shared controller belongs to Classic: stop NimBLE first, then the
    # Classic host/controller.  No in-flight command may survive unregister.
    peer.write("e\n")
    peer.expect_exact("DUAL_PEER_ENDED ble=0 classic=0 busy=0", timeout=20)
    dut.write("e")
    dut.expect_exact("DUAL_ENDED ble=0 classic=0 busy=0", timeout=20)

    for _ in range(3):
        peer.write("s\n")
        peer.expect_exact(
            "DUAL_PEER_RESTART started=1 ble=1 classic=1 busy=0", timeout=30
        )
        dut.write("s")
        dut.expect_exact(
            "DUAL_RESTART started=1 ble=1 classic=1 busy=0", timeout=30
        )

        peer.write("e\n")
        peer.expect_exact("DUAL_PEER_ENDED ble=0 classic=0 busy=0", timeout=20)
        dut.write("e")
        dut.expect_exact("DUAL_ENDED ble=0 classic=0 busy=0", timeout=20)
