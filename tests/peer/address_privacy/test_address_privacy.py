import re


def test_peripheral_uses_random_static_address(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    match = dut.expect(re.compile(rb"PEER_SEEN addr=([0-9a-fA-F:]+) type=(\d+)"), timeout=20)

    address = match.group(1).decode()
    address_type = int(match.group(2))

    # The peer advertises with a Random address (type 1), not its Public address.
    assert address_type == 1, f"expected Random address type, got {address_type}"

    # A static random address has the two most-significant bits set (0b11) in the
    # most-significant octet, distinguishing it from resolvable/non-resolvable
    # private addresses.
    most_significant_octet = int(address.split(":")[0], 16)
    assert (most_significant_octet & 0xC0) == 0xC0, (
        f"expected a static random address (top bits 0b11), got {address}"
    )
