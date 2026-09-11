import re


def parse_address(match):
    return match.group(1).decode(), int(match.group(2))


def assert_rpa(address, address_type):
    assert address_type == 1, f"expected random OTA address, got type {address_type}"
    most_significant_octet = int(address.split(":")[0], 16)
    assert (most_significant_octet & 0xC0) == 0x40, (
        f"expected resolvable private address (top bits 0b01), got {address}"
    )


def start_connection(dut):
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    seen = dut.expect(
        re.compile(rb"RPA_SEEN addr=([0-9a-f:]+) type=(\d+)"), timeout=20
    )
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    return parse_address(seen)


def expect_secure_connection(dut, peripheral, connection_id, initial_value):
    dut.expect_exact(
        f"CENTRAL_CONNECTED id={connection_id} encrypted=0 bonded=0",
        timeout=20,
    )
    central_peer = dut.expect(
        re.compile(rb"RPA_CENTRAL_PEER addr=([0-9a-f:]+) type=(\d+)"),
        timeout=10,
    )
    peripheral_peer = peripheral.expect(
        re.compile(rb"RPA_PERIPHERAL_PEER addr=([0-9a-f:]+) type=(\d+)"),
        timeout=20,
    )
    assert_rpa(*parse_address(central_peer))
    assert_rpa(*parse_address(peripheral_peer))

    dut.expect_exact(
        "CENTRAL_SECURITY success=1 encrypted=1 authenticated=0 bonded=1 "
        "key=16 stored=1 context=loop",
        timeout=20,
    )
    peripheral.expect_exact(
        "PERIPHERAL_SECURITY success=1 encrypted=1 authenticated=0 bonded=1 "
        "key=16 stored=1 context=loop",
        timeout=20,
    )
    dut.expect_exact("DISCOVER_REQUESTED", timeout=10)
    dut.expect_exact("DISCOVER success=1 context=loop", timeout=20)
    dut.expect_exact("READ_REQUESTED", timeout=10)
    dut.expect_exact(
        f"READ success=1 value={initial_value} context=loop", timeout=20
    )
    dut.expect_exact("WRITE_REQUESTED", timeout=10)
    dut.expect_exact("WRITE success=1 context=loop", timeout=20)
    peripheral.expect_exact(
        "SECURE_WRITE value=central-secure-write encrypted=1 bonded=1 "
        "context=loop",
        timeout=20,
    )


def disconnect(dut, peripheral, connection_id):
    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact(
        f"CENTRAL_DISCONNECTED id={connection_id} context=loop", timeout=20
    )
    peripheral.expect_exact(
        f"PERIPHERAL_DISCONNECTED id={connection_id} context=loop", timeout=20
    )


def clear_bonds(dut, peripheral):
    dut.write("x")
    peripheral.write("x")
    dut.expect_exact("CENTRAL_BONDS_CLEARED success=1 count=0", timeout=10)
    peripheral.expect_exact(
        "PERIPHERAL_BONDS_CLEARED success=1 count=0", timeout=10
    )


def test_rpa_bond_restore_delete_and_repair(dut, peers):
    peripheral = peers["device"]
    dut.write("?")
    peripheral.write("R")
    dut.expect(re.compile(rb"RPA_CENTRAL_READY local=[0-9a-f:]+ type=1"), timeout=20)
    peripheral.expect(
        re.compile(rb"RPA_PERIPHERAL_READY local=[0-9a-f:]+ type=1"), timeout=20
    )

    clear_bonds(dut, peripheral)
    initial_scan_address, initial_scan_type = start_connection(dut)
    assert_rpa(initial_scan_address, initial_scan_type)
    expect_secure_connection(dut, peripheral, 1, "secure-ready")
    disconnect(dut, peripheral, 1)

    # Restart both hosts with the bond still in NVS. This makes the next start
    # restore both IRKs into each host-based resolving list.
    dut.write("r")
    peripheral.write("r")
    dut.expect_exact("RPA_CENTRAL_RESTARTING", timeout=10)
    peripheral.expect_exact("RPA_PERIPHERAL_RESTARTING", timeout=10)
    dut.expect(re.compile(rb"RPA_CENTRAL_READY local=[0-9a-f:]+ type=1"), timeout=30)
    peripheral.expect(
        re.compile(rb"RPA_PERIPHERAL_READY local=[0-9a-f:]+ type=1"), timeout=30
    )

    dut.write("b")
    peripheral.write("b")
    dut.expect_exact("CENTRAL_BONDS count=1", timeout=10)
    peripheral.expect_exact("PERIPHERAL_BONDS count=1", timeout=10)

    restored_scan_address, restored_scan_type = start_connection(dut)
    print(
        f"RPA_RESTORED_SCAN addr={restored_scan_address} "
        f"type={restored_scan_type}"
    )
    expect_secure_connection(dut, peripheral, 1, "secure-ready")
    disconnect(dut, peripheral, 1)

    # This is the former failure path: after IRK restoration the peer-device
    # record can be absent, but deleteAllBonds must still remove the resolving
    # list entry. A fresh pairing must not hit duplicate-entry EINVAL.
    clear_bonds(dut, peripheral)
    peripheral.write("a")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)
    repaired_scan_address, repaired_scan_type = start_connection(dut)
    assert_rpa(repaired_scan_address, repaired_scan_type)
    expect_secure_connection(dut, peripheral, 2, "central-secure-write")
    disconnect(dut, peripheral, 2)
    clear_bonds(dut, peripheral)
