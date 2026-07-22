import re


def test_service_data_broadcast_and_read(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    match = dut.expect(
        re.compile(rb"SERVICE_DATA uuid=(\S+) data=([0-9a-f]+)"), timeout=20
    )

    uuid = match.group(1).decode().lower()
    data = match.group(2).decode()

    # The peer advertised payload AB CD EF 12 under 16-bit UUID 0xFEAB.
    assert data == "abcdef12", f"unexpected service data {data}"
    assert "feab" in uuid, f"service data UUID missing feab: {uuid}"
