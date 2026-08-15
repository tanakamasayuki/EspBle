def test_custom_classic_host_initializes_hid_device_and_host(dut):
    dut.expect_exact("CLASSIC_HIDD_START 0 0", timeout=20)
    dut.expect_exact("CLASSIC_HIDH_START 0 0", timeout=20)
    dut.expect_exact("CLASSIC_HIDD_INIT status=0", timeout=20)
    dut.expect_exact("CLASSIC_HIDH_INIT status=0", timeout=20)
    dut.expect_exact("CLASSIC_HID_DEINIT_START 0 0", timeout=20)
    dut.expect_exact("CLASSIC_HIDD_DEINIT status=0", timeout=20)
    dut.expect_exact("CLASSIC_HIDH_DEINIT status=0", timeout=20)
