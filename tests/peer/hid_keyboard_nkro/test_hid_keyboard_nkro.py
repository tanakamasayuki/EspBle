def test_hid_keyboard_nkro(dut, peers):
    device = peers["device"]
    dut.write("x")
    device.write("x")
    dut.expect_exact("HOST_BONDS_CLEARED success=1", timeout=10)
    device.expect_exact("DEVICE_BONDS_CLEARED success=1", timeout=10)

    dut.write("s")
    dut.expect_exact("HOST_SCAN_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_CONNECT_STARTED success=1", timeout=20)
    dut.expect_exact("HOST_CONNECTED id=1", timeout=20)
    dut.expect_exact("HOST_DISCOVERY_STARTED success=1", timeout=20)
    dut.expect_exact("HOST_DISCOVERED success=1 report=1 output=1 detail=", timeout=20)

    device.write("n")
    device.expect_exact("DEVICE_NKRO_SENT success=1", timeout=20)
    dut.expect_exact("HOST_NKRO_STATE count=8 high=1 b=1 b_released=0", timeout=20)

    device.write("b")
    device.expect_exact("DEVICE_RELEASE_USAGE success=1", timeout=10)
    dut.expect_exact("HOST_NKRO_STATE count=7 high=1 b=0 b_released=1", timeout=20)

    dut.write("l")
    dut.expect_exact("HOST_LEDS_WRITTEN success=1", timeout=10)
    device.expect_exact("DEVICE_OUTPUT leds=3", timeout=20)

    device.write("r")
    device.expect_exact("DEVICE_RELEASE_ALL success=1", timeout=10)
    dut.expect_exact("HOST_NKRO_STATE count=0 high=0 b=0 b_released=0", timeout=20)
