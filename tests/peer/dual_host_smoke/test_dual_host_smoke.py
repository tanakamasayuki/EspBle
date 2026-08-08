import os
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

    peer.write("g\n")
    peer.expect_exact("DUAL_BLE_SCAN 1", timeout=10)
    peer.expect_exact("DUAL_BLE_CONNECT 1", timeout=20)
    peer.expect_exact("DUAL_BLE_CLIENT_CONNECTED", timeout=20)
    dut.expect_exact("DUAL_BLE_SERVER_CONNECTED", timeout=20)
    peer.expect_exact("DUAL_BLE_READ_REQUESTED 1", timeout=20)
    peer.expect_exact(
        "DUAL_BLE_CLIENT_SECURITY success=1 encrypted=1 bonded=1 key=16 classic=1",
        timeout=30,
    )
    dut.expect_exact(
        "DUAL_BLE_SERVER_SECURITY success=1 encrypted=1 bonded=1 key=16 classic=1",
        timeout=30,
    )
    peer.expect_exact("DUAL_BLE_READ success=1 value=dual-ready classic=1", timeout=20)

    # Cross the controller's former 20-packet host-buffer limit by a wide
    # margin.  One initial read plus these repeats generates over 50 LE ACL
    # packets across the two directions.
    gatt_read_repeats = int(os.getenv("ESPBLE_DUAL_GATT_READ_REPEATS", "24"))
    assert 0 <= gatt_read_repeats <= 1000
    for _ in range(gatt_read_repeats):
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

    dut.write("n")
    peer.write("n\n")
    dut.expect_exact("DUAL_BLE_BONDS 1", timeout=10)
    peer.expect_exact("DUAL_BLE_BONDS 1", timeout=10)

    # Reconnect LE from its bond while the Classic HID ACL link remains live.
    peer.write("k\n")
    peer.expect_exact("DUAL_BLE_DISCONNECT 1", timeout=10)
    peer.expect_exact("DUAL_BLE_CLIENT_DISCONNECTED", timeout=20)
    dut.expect_exact("DUAL_BLE_SERVER_DISCONNECTED", timeout=20)
    dut.write("a")
    dut.expect_exact("DUAL_BLE_ADVERTISING 1", timeout=10)
    peer.write("g\n")
    peer.expect_exact("DUAL_BLE_SCAN 1", timeout=10)
    peer.expect_exact("DUAL_BLE_CONNECT 1", timeout=20)
    peer.expect_exact("DUAL_BLE_CLIENT_CONNECTED", timeout=20)
    dut.expect_exact("DUAL_BLE_SERVER_CONNECTED", timeout=20)
    peer.expect_exact("DUAL_BLE_READ_REQUESTED 1", timeout=20)
    dut.expect_exact(
        "DUAL_BLE_SERVER_SECURITY success=1 encrypted=1 bonded=1 key=16 classic=1",
        timeout=30,
    )
    peer.expect_exact(
        "DUAL_BLE_CLIENT_SECURITY success=1 encrypted=1 bonded=1 key=16 classic=1",
        timeout=30,
    )
    peer.expect_exact(
        "DUAL_BLE_READ success=1 value=dual-ready classic=1", timeout=20
    )
    peer.write("u\n")
    link = peer.expect(re.compile(rb"DUAL_BLE_LINK .*"), timeout=10)
    print(link.group(0).decode())
    assert re.fullmatch(
        rb"DUAL_BLE_LINK found=1 encrypted=1 bonded=1 "
        rb"security_events=2 event=08 status=0 enabled=1",
        link.group(0),
    )

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
        masks = re.search(rb"masks=(\d+)/(\d+)", diag)
        assert masks is not None
        assert int(masks.group(1)) >= 3
        assert int(masks.group(2)) >= 1

    dut.write("v")
    peer.write("v\n")
    opcode_lines = []
    for prefix, device in ((rb"DUAL_OPCODES", dut), (rb"DUAL_PEER_OPCODES", peer)):
        for host in range(2):
            match = device.expect(
                re.compile(
                    prefix
                    + rb" host="
                    + str(host).encode()
                    + rb" count=(\d+) overflow=(\d+) values=([0-9a-f,]*)"
                ),
                timeout=10,
            )
            assert int(match.group(1)) > 0
            assert match.group(2) == b"0"
            opcode_lines.append(match.group(0).decode())
    print("\n".join(opcode_lines))

    dut.write("?")
    dut.expect_exact("DUAL_STATE adv=0 classic=1 ble=1", timeout=10)

    # The broker owns controller shutdown.  Classic may leave first while the
    # physical controller remains available to NimBLE.
    peer.write("x\n")
    peer.expect_exact(
        "DUAL_PEER_REVERSE ble=1 classic=0 error=None", timeout=20
    )
    dut.write("x")
    dut.expect_exact("DUAL_REVERSE ble=1 classic=0 error=None", timeout=20)

    peer.write("r\n")
    peer.expect_exact("DUAL_BLE_READ_REQUESTED 1", timeout=10)
    peer.expect_exact(
        "DUAL_BLE_READ success=1 value=dual-ready classic=0", timeout=10
    )

    # Reattaching Bluedroid sends HCI Reset as part of its normal bootstrap.
    # The broker must complete it virtually instead of resetting the live LE
    # controller state.
    peer.write("y\n")
    peer.expect_exact(
        "DUAL_PEER_CLASSIC_REATTACH started=1 resets=1 flow=2", timeout=30
    )
    dut.write("y")
    dut.expect_exact(
        "DUAL_CLASSIC_REATTACH started=1 resets=1 flow=2", timeout=30
    )

    peer.write("r\n")
    peer.expect_exact("DUAL_BLE_READ_REQUESTED 1", timeout=10)
    peer.expect_exact(
        "DUAL_BLE_READ success=1 value=dual-ready classic=0", timeout=10
    )

    peer.write(b"c" + ready.group(1) + b"\n")
    peer.expect_exact("DUAL_PEER_CONNECT 1", timeout=10)
    dut.expect_exact("DUAL_CLASSIC_CONNECTED", timeout=30)
    peer.expect_exact("DUAL_PEER_CONNECTED", timeout=30)
    dut.write("i")
    dut.expect_exact("DUAL_CLASSIC_INPUT 1", timeout=10)
    peer.expect(re.compile(rb"DUAL_PEER_INPUT hex=(01)?007f80[0-9a-f]{2}"), timeout=20)
    peer.expect_exact("DUAL_PEER_OUTPUT 1", timeout=10)
    dut.expect(re.compile(rb"DUAL_CLASSIC_OUTPUT id=2 hex=(a500ff|02a500ff)"), timeout=20)

    # Issue LE and BR/EDR Disconnect commands back-to-back from different
    # logical hosts.  Both handle-owned completions must survive either order.
    peer.write("q\n")
    peer.expect_exact(
        "DUAL_PEER_DUAL_DISCONNECT ble=1 classic=1", timeout=10
    )
    peer_disconnects = {
        peer.expect(
            re.compile(
                rb"DUAL_(?:BLE_CLIENT_DISCONNECTED|PEER_DISCONNECTED)"
            ),
            timeout=20,
        ).group(0)
        for _ in range(2)
    }
    dut_disconnects = {
        dut.expect(
            re.compile(
                rb"DUAL_(?:BLE_SERVER_DISCONNECTED|CLASSIC_DISCONNECTED)"
            ),
            timeout=20,
        ).group(0)
        for _ in range(2)
    }
    assert peer_disconnects == {
        b"DUAL_BLE_CLIENT_DISCONNECTED",
        b"DUAL_PEER_DISCONNECTED",
    }
    assert dut_disconnects == {
        b"DUAL_BLE_SERVER_DISCONNECTED",
        b"DUAL_CLASSIC_DISCONNECTED",
    }

    # Stop both rejoined hosts. The final unregister must trigger the broker's
    # adopted controller-stop callback, with no command surviving the session.
    peer.write("e\n")
    peer.expect_exact("DUAL_PEER_ENDED ble=0 classic=0 busy=0", timeout=20)
    dut.write("e")
    dut.expect_exact("DUAL_ENDED ble=0 classic=0 busy=0", timeout=20)

    restart_cycles = int(os.getenv("ESPBLE_DUAL_RESTART_CYCLES", "3"))
    assert 1 <= restart_cycles <= 100
    for _ in range(restart_cycles):
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

    # Exercise the actual C++ destructors in both object orders, then prove the
    # controller was fully released by starting the long-lived instances again.
    peer.write("z\n")
    peer.expect_exact(
        "DUAL_PEER_DESTRUCT classic_first=1 ble_first=1 restarted=1", timeout=40
    )
    dut.write("z")
    dut.expect_exact(
        "DUAL_DESTRUCT classic_first=1 ble_first=1 restarted=1", timeout=40
    )

    peer.write("e\n")
    peer.expect_exact("DUAL_PEER_ENDED ble=0 classic=0 busy=0", timeout=20)
    dut.write("e")
    dut.expect_exact("DUAL_ENDED ble=0 classic=0 busy=0", timeout=20)
