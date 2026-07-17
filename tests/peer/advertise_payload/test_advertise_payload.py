import re

PAYLOAD_PATTERN = re.compile(rb"SCANNER_PAYLOAD len=(\d+) hex=([0-9a-f]+)")


def _parse_ad_structures(payload: bytes):
    structures = []
    offset = 0
    while offset < len(payload):
        length = payload[offset]
        if length == 0:
            break
        assert offset + 1 + length <= len(payload), "truncated AD structure"
        ad_type = payload[offset + 1]
        data = payload[offset + 2 : offset + 1 + length]
        structures.append((ad_type, data))
        offset += 1 + length
    return structures


def test_16bit_service_uuids_share_one_ad_structure(dut, peers):
    """CSS Part A 1.1: a data type must not appear more than once in an
    advertising payload; both 16-bit UUIDs must share one Complete List."""
    device = peers["device"]

    device.write("a")
    device.expect_exact("DEVICE_ADVERTISE_STARTED success=1", timeout=10)
    dut.write("s")
    dut.expect_exact("SCANNER_STARTED success=1", timeout=10)
    match = dut.expect(PAYLOAD_PATTERN, timeout=30)
    payload = bytes.fromhex(match.group(2).decode())

    structures = _parse_ad_structures(payload)
    complete16 = [data for ad_type, data in structures if ad_type == 0x03]
    assert len(complete16) == 1, (
        f"expected one Complete List of 16-bit Service UUIDs, got "
        f"{len(complete16)} (structures: "
        f"{[(hex(t), d.hex()) for t, d in structures]})"
    )
    uuids = {
        int.from_bytes(complete16[0][index : index + 2], "little")
        for index in range(0, len(complete16[0]), 2)
    }
    assert uuids == {0x1812, 0x180F}, f"unexpected UUID list: {uuids}"

    # No AD type may appear more than once in the same payload.
    types = [ad_type for ad_type, _ in structures]
    assert len(types) == len(set(types)), f"duplicate AD types: {types}"
