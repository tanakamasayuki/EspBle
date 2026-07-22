def test_eddystone_url_uid_tlm(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    # URL frame (default): "https://www.example.com/" encodes as scheme 0x01 +
    # "example" + 0x00 (.com/); tx power -20; non-connectable, non-scannable.
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact(
        "EDDYSTONE_URL url=https://www.example.com/ tx=-20 connectable=0 scannable=0",
        timeout=20,
    )

    # UID frame: namespace 0xA0..0xA9, instance 0x10..0x15, tx power -18.
    peripheral.write("u")
    peripheral.expect_exact("FRAME_UID", timeout=10)
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact(
        "EDDYSTONE_UID namespace=a0a1a2a3a4a5a6a7a8a9 instance=101112131415 tx=-18",
        timeout=20,
    )

    # TLM frame: 3300 mV, 25.5 C (printed as tenths = 255), adv count 66051,
    # uptime 168496141 (0.1 s units).
    peripheral.write("t")
    peripheral.expect_exact("FRAME_TLM", timeout=10)
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact(
        "EDDYSTONE_TLM battery=3300 temp=255 count=66051 uptime=168496141",
        timeout=20,
    )
