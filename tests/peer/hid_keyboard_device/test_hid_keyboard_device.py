def test_hid_keyboard_device_reports(dut, peers):
    central = dut
    keyboard_device = peers["device"]

    keyboard_device.write("x")
    central.write("x")
    keyboard_device.expect_exact("HID_BONDS_CLEARED success=1 count=0", timeout=10)
    central.expect_exact("PEER_BONDS_CLEARED count=0", timeout=10)

    keyboard_device.write("?")
    keyboard_device.expect_exact("ADVERTISING 1", timeout=10)
    central.write("s")
    central.expect_exact("HID_SCAN_STARTED", timeout=10)
    central.expect_exact("PEER_CONNECTED", timeout=20)
    keyboard_device.expect_exact("HID_CONNECTED id=1", timeout=20)
    central.expect_exact("PEER_SECURITY encrypted=1 bonded=1", timeout=20)
    keyboard_device.expect_exact("HID_SECURITY encrypted=1 bonded=1 context=loop", timeout=20)
    central.expect_exact("REPORT_MAP valid=1", timeout=20)
    central.expect_exact("HID_REPORTS input=1 output=1 battery=87", timeout=20)
    central.expect_exact("INPUT_SUBSCRIBED success=1", timeout=20)
    central.expect_exact("HID_READY", timeout=10)

    keyboard_device.write("k")
    keyboard_device.expect_exact("INPUT_SENT", timeout=10)
    central.expect_exact("INPUT_REPORT length=8 modifiers=2 key0=4", timeout=20)

    keyboard_device.write("r")
    keyboard_device.expect_exact("RELEASE_SENT", timeout=10)
    central.expect_exact("INPUT_REPORT length=8 modifiers=0 key0=0", timeout=20)

    central.write("l")
    central.expect_exact("OUTPUT_WRITE success=1", timeout=20)
    keyboard_device.expect_exact(
        "OUTPUT_REPORT id=1 leds=3 num=1 caps=1 context=loop", timeout=20
    )

    central.write("d")
    central.expect_exact("PEER_DISCONNECTED", timeout=10)
    keyboard_device.expect_exact("HID_DISCONNECTED id=1 context=loop", timeout=20)

    keyboard_device.write("x")
    central.write("x")
    keyboard_device.expect_exact("HID_BONDS_CLEARED success=1 count=0", timeout=10)
    central.expect_exact("PEER_BONDS_CLEARED count=0", timeout=10)
