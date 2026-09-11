import re


def test_bond_management_service(dut, peers):
    """The Bond Management Service server exposes a readable Bond Management
    Feature bit field (uint24) and a writable Bond Management Control Point. The
    client reads the Feature (0x000011: delete-LE-bond + delete-all-bonds
    supported) and writes op code 0x03 (Delete bond of requesting device, LE)
    with response; the server observes the op code in onWritten. Validates the
    GATT choreography, not actual bond deletion."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("FEATURE_READ_REQUESTED", timeout=20)

    feature = dut.expect(re.compile(rb"FEATURE_READ valid=(\d+) value=([0-9a-f]{6}) context=(\w+)"), timeout=20)
    assert feature.group(1) == b"1", "Bond Management Feature read failed"
    assert feature.group(2) == b"000011", "expected delete-LE-bond + delete-all-bonds features"
    assert feature.group(3) == b"loop"

    # Write op code 0x03 (Delete bond of requesting device, LE) with response.
    dut.write("x")
    dut.expect_exact("CONTROL_WRITE_REQUESTED", timeout=10)
    control = device.expect(
        re.compile(rb"CONTROL_WRITE opcode=(\d+) length=(\d+) context=(\w+)"), timeout=20)
    assert int(control.group(1)) == 3, "server should receive op code 3"
    assert int(control.group(2)) == 1, "op code write should be 1 byte"
    assert control.group(3) == b"loop"
    dut.expect_exact("CONTROL_WRITTEN success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
