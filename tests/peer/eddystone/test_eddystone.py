def test_eddystone_url_broadcast_and_decode(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    # "https://www.example.com/" encodes as scheme 0x01 + "example" + 0x00 (.com/);
    # tx power -20; broadcast non-connectable, non-scannable.
    dut.expect_exact(
        "EDDYSTONE url=https://www.example.com/ tx=-20 connectable=0 scannable=0",
        timeout=20,
    )
