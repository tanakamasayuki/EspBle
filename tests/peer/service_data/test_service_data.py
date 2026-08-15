import re

# Anchored to the end of the line: the data field is the last one and varies in
# length, so a pattern without the newline can match while the rest of the line
# is still arriving and capture a value that is one character short.
BLOCK = re.compile(rb"SERVICE_DATA index=(\d+) uuid=(\S+) data=([0-9a-f]*)\r?\n")


def test_service_data_blocks_are_received_and_looked_up(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)

    # The peer advertises two blocks: AB CD EF 12 under 0xFEAB and 2E 09 under
    # 0x181A.
    dut.expect_exact("SERVICE_DATA_COUNT 2", timeout=20)

    blocks = {}
    for _ in range(2):
        match = dut.expect(BLOCK, timeout=10)
        blocks[match.group(2).decode().lower()] = match.group(3).decode()

    feab = [data for uuid, data in blocks.items() if "feab" in uuid]
    esss = [data for uuid, data in blocks.items() if "181a" in uuid]
    assert feab == ["abcdef12"], f"unexpected 0xFEAB block: {blocks}"
    assert esss == ["2e09"], f"unexpected 0x181A block: {blocks}"

    # serviceDataFor() compares UUIDs by value, so the 16-bit shorthand "181A"
    # matches the 128-bit form the scan result carries.
    dut.expect_exact("SERVICE_DATA_LOOKUP data=2e09", timeout=10)
