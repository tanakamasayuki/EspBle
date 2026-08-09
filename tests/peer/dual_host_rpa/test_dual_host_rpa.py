import os
import re
import time


def parse_address(match):
    return match.group(1).decode(), int(match.group(2))


def assert_rpa(address, address_type):
    assert address_type == 1, f"expected random OTA address, got type {address_type}"
    assert int(address.split(":")[0], 16) & 0xC0 == 0x40, (
        f"expected resolvable private address (top bits 0b01), got {address}"
    )


def expect_secure_read(dut, peer):
    peer.expect_exact("DUAL_BLE_CLIENT_CONNECTED", timeout=20)
    client_peer = peer.expect(
        re.compile(rb"RPA_DUAL_CLIENT_PEER addr=([0-9a-f:]+) type=(\d+)"),
        timeout=10,
    )
    dut.expect_exact("DUAL_BLE_SERVER_CONNECTED", timeout=20)
    server_peer = dut.expect(
        re.compile(rb"RPA_DUAL_SERVER_PEER addr=([0-9a-f:]+) type=(\d+)"),
        timeout=10,
    )
    assert_rpa(*parse_address(client_peer))
    assert_rpa(*parse_address(server_peer))
    client_peer_address = parse_address(client_peer)[0]
    server_peer_address = parse_address(server_peer)[0]
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
    return client_peer_address, server_peer_address


def start_rpa_connection(peer):
    peer.write("g\n")
    peer.expect_exact("DUAL_BLE_SCAN 1", timeout=10)
    seen = peer.expect(
        re.compile(rb"RPA_DUAL_SEEN addr=([0-9a-f:]+) type=(\d+)"), timeout=20
    )
    assert_rpa(*parse_address(seen))
    peer.expect_exact("DUAL_BLE_CONNECT 1", timeout=20)
    return parse_address(seen)[0]


def disconnect_ble(dut, peer):
    peer.write("k\n")
    peer.expect_exact("DUAL_BLE_DISCONNECT 1", timeout=10)
    peer.expect_exact("DUAL_BLE_CLIENT_DISCONNECTED", timeout=20)
    dut.expect_exact("DUAL_BLE_SERVER_DISCONNECTED", timeout=20)
    dut.write("a")
    dut.expect_exact("DUAL_BLE_ADVERTISING 1", timeout=10)


def expect_classic_hid_round_trip(dut, peer):
    dut.write("i")
    dut.expect_exact("DUAL_CLASSIC_INPUT 1", timeout=10)
    peer.expect(
        re.compile(rb"DUAL_PEER_INPUT hex=(01)?007f80[0-9a-f]{2}"), timeout=10
    )
    peer.expect_exact("DUAL_PEER_OUTPUT 1", timeout=10)
    dut.expect(
        re.compile(rb"DUAL_CLASSIC_OUTPUT id=2 hex=(a500ff|02a500ff)"), timeout=10
    )


def expect_clean_broker_and_rpa_opcode(device, diag_command, opcode_command, prefix):
    device.write(diag_command)
    diagnostics = device.expect(
        re.compile(prefix + rb"_DIAG .* masks=\d+/\d+"), timeout=10
    ).group(0)
    for clean_field in (b"unknown=0", b"qfull=0", b"mismatch=0", b"busy=0"):
        assert clean_field in diagnostics

    device.write(opcode_command)
    opcodes = device.expect(
        re.compile(prefix + rb"_OPCODES host=0 .* values=([0-9a-f,]+)"),
        timeout=10,
    ).group(1).split(b",")
    assert b"2005" in opcodes, "LE Set Random Address was not routed to NimBLE"


def test_rpa_bond_reconnect_and_reboot_restore_with_classic_hid(dut, peers):
    peer = peers["device"]
    # Query readiness explicitly because some USB-UART bridges can finish the
    # sketch setup before pytest attaches its serial reader after flashing.
    dut.write("R")
    peer.write("R\n")
    peer.expect(
        re.compile(rb"DUAL_PEER_READY(?: local=[0-9a-f:]+ type=1)?"), timeout=20
    )
    ready = dut.expect(
        re.compile(rb"DUAL_READY classic=([0-9a-f:]+) ble=([0-9a-f:]+) type=(\d+)"),
        timeout=30,
    )
    assert ready.group(3) == b"1"

    dut.write("X")
    peer.write("X\n")
    dut.expect_exact("RPA_DUAL_BONDS_CLEARED success=1 count=0", timeout=10)
    peer.expect_exact(
        "RPA_DUAL_PEER_BONDS_CLEARED success=1 count=0", timeout=10
    )

    peer.write(b"c" + ready.group(1) + b"\n")
    peer.expect_exact("DUAL_PEER_CONNECT 1", timeout=10)
    dut.expect_exact("DUAL_CLASSIC_CONNECTED", timeout=30)
    peer.expect_exact("DUAL_PEER_CONNECTED", timeout=30)

    initial_rpa = start_rpa_connection(peer)
    _, initial_central_rpa = expect_secure_read(dut, peer)
    dut.write("n")
    peer.write("n\n")
    dut.expect_exact("DUAL_BLE_BONDS 1", timeout=10)
    peer.expect_exact("DUAL_BLE_BONDS 1", timeout=10)

    disconnect_ble(dut, peer)

    start_rpa_connection(peer)
    expect_secure_read(dut, peer)
    dut.write("?")
    dut.expect_exact("DUAL_STATE adv=0 classic=1 ble=1", timeout=10)

    # Exercise the host privacy callout itself. It preempts advertising, sends
    # HCI Set Random Address through the broker, then resumes advertising.
    disconnect_ble(dut, peer)
    dut.write("T")
    dut.expect_exact("RPA_DUAL_TIMEOUT seconds=2 rc=0", timeout=10)
    rotation_cycles = int(os.getenv("ESPBLE_DUAL_RPA_ROTATION_CYCLES", "3"))
    assert 1 <= rotation_cycles <= 20
    rotated_rpas = []
    previous_rpa = initial_rpa
    for _ in range(rotation_cycles):
        time.sleep(3)
        peer.write("p\n")
        peer.expect_exact("RPA_DUAL_OBSERVE 1", timeout=10)
        rotated = peer.expect(
            re.compile(rb"RPA_DUAL_OBSERVED addr=([0-9a-f:]+) type=(\d+)"),
            timeout=20,
        )
        rotated_rpa, rotated_type = parse_address(rotated)
        assert_rpa(rotated_rpa, rotated_type)
        assert rotated_rpa != previous_rpa
        assert rotated_rpa not in rotated_rpas
        rotated_rpas.append(rotated_rpa)
        previous_rpa = rotated_rpa
        expect_classic_hid_round_trip(dut, peer)
    dut.write("Y")
    dut.expect_exact("RPA_DUAL_TIMEOUT seconds=900 rc=0", timeout=10)

    assert start_rpa_connection(peer) == rotated_rpas[-1]
    expect_secure_read(dut, peer)
    dut.write("?")
    dut.expect_exact("DUAL_STATE adv=0 classic=1 ble=1", timeout=10)

    # Rotate the Central's RPA while it is scanning. Keep the Peripheral quiet
    # until after the timer fires, so receiving it proves the preempted scan was
    # restarted rather than merely having reported before rotation.
    disconnect_ble(dut, peer)
    dut.write("S")
    dut.expect_exact("RPA_DUAL_ADVERTISING_STOP 1", timeout=10)
    peer.write("t\n")
    peer.expect_exact("RPA_DUAL_PEER_TIMEOUT seconds=2 rc=0", timeout=10)
    peer.write("p\n")
    peer.expect_exact("RPA_DUAL_OBSERVE 1", timeout=10)
    # Cross the same number of timer expirations without stopping the scan.
    # Advertising only starts afterwards, so a result proves the scan survived
    # every privacy preemption. Classic HID traffic remains live meanwhile.
    for _ in range(rotation_cycles):
        time.sleep(3)
        expect_classic_hid_round_trip(dut, peer)
    peer.write("y\n")
    peer.expect_exact("RPA_DUAL_PEER_TIMEOUT seconds=900 rc=0", timeout=10)
    dut.write("a")
    dut.expect_exact("DUAL_BLE_ADVERTISING 1", timeout=10)
    observed = peer.expect(
        re.compile(rb"RPA_DUAL_OBSERVED addr=([0-9a-f:]+) type=(\d+)"),
        timeout=20,
    )
    assert_rpa(*parse_address(observed))
    start_rpa_connection(peer)
    _, rotated_central_rpa = expect_secure_read(dut, peer)
    assert rotated_central_rpa != initial_central_rpa
    dut.write("?")
    dut.expect_exact("DUAL_STATE adv=0 classic=1 ble=1", timeout=10)

    # A privacy restart must consume the remaining finite duration, not begin
    # the original duration again. Eight seconds crosses three two-second RPA
    # rotations; it must still be active at three seconds and expired at nine.
    disconnect_ble(dut, peer)
    dut.write("T")
    dut.expect_exact("RPA_DUAL_TIMEOUT seconds=2 rc=0", timeout=10)
    dut.write("F")
    dut.expect_exact(
        "RPA_DUAL_FINITE_ADVERTISING seconds=8 success=1", timeout=10
    )
    time.sleep(3)
    dut.write("?")
    dut.expect_exact("DUAL_STATE adv=1 classic=1 ble=0", timeout=10)
    expect_classic_hid_round_trip(dut, peer)
    time.sleep(6)
    dut.write("?")
    dut.expect_exact("DUAL_STATE adv=0 classic=1 ble=0", timeout=10)
    dut.write("Y")
    dut.expect_exact("RPA_DUAL_TIMEOUT seconds=900 rc=0", timeout=10)

    peer.write("t\n")
    peer.expect_exact("RPA_DUAL_PEER_TIMEOUT seconds=2 rc=0", timeout=10)
    peer.write("f\n")
    peer.expect_exact("RPA_DUAL_FINITE_SCAN seconds=8 success=1", timeout=10)
    time.sleep(3)
    peer.write("w\n")
    peer.expect_exact("RPA_DUAL_SCAN_STATE active=1", timeout=10)
    expect_classic_hid_round_trip(dut, peer)
    time.sleep(6)
    peer.write("w\n")
    peer.expect_exact("RPA_DUAL_SCAN_STATE active=0", timeout=10)
    peer.write("y\n")
    peer.expect_exact("RPA_DUAL_PEER_TIMEOUT seconds=900 rc=0", timeout=10)

    # Restore the connected state used by the reboot/bond restoration case.
    dut.write("a")
    dut.expect_exact("DUAL_BLE_ADVERTISING 1", timeout=10)
    start_rpa_connection(peer)
    expect_secure_read(dut, peer)
    dut.write("?")
    dut.expect_exact("DUAL_STATE adv=0 classic=1 ble=1", timeout=10)
    expect_clean_broker_and_rpa_opcode(dut, "d", "v", rb"DUAL")
    expect_clean_broker_and_rpa_opcode(peer, "d\n", "v\n", rb"DUAL_PEER")

    # Reboot both complete dual-host stacks with their IRK/LTK in NVS. Classic
    # necessarily drops across reset; restore it first, then prove that LE uses
    # the existing bond rather than requiring a fresh identity.
    dut.write("Z")
    peer.write("Z\n")
    dut.expect_exact("RPA_DUAL_RESTARTING", timeout=10)
    peer.expect_exact("RPA_DUAL_PEER_RESTARTING", timeout=10)
    restarted = dut.expect(
        re.compile(rb"DUAL_READY classic=([0-9a-f:]+) ble=([0-9a-f:]+) type=1"),
        timeout=30,
    )
    peer.expect_exact("DUAL_PEER_READY", timeout=30)

    dut.write("n")
    peer.write("n\n")
    dut.expect_exact("DUAL_BLE_BONDS 1", timeout=10)
    peer.expect_exact("DUAL_BLE_BONDS 1", timeout=10)

    peer.write(b"c" + restarted.group(1) + b"\n")
    peer.expect_exact("DUAL_PEER_CONNECT 1", timeout=10)
    dut.expect_exact("DUAL_CLASSIC_CONNECTED", timeout=30)
    peer.expect_exact("DUAL_PEER_CONNECTED", timeout=30)
    start_rpa_connection(peer)
    expect_secure_read(dut, peer)
    dut.write("?")
    dut.expect_exact("DUAL_STATE adv=0 classic=1 ble=1", timeout=10)
