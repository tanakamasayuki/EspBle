import re


def test_hid_boot_protocol(dut, peers):
    """Exercise the HID over GATT Boot Protocol on a keyboard peripheral. The
    generic GATT client reads Protocol Mode (default Report = 1), subscribes to
    the Boot Keyboard Input Report, switches the device to Boot Protocol Mode,
    receives an 8-byte boot report for Shift+'a', and writes the Boot Keyboard
    Output Report Caps Lock LED, which the device observes via onOutputReport."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1 mode=1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect(re.compile(rb"CONNECTED id=(\d+)"), timeout=20)
    device.expect(re.compile(rb"HID_CONNECTED id=(\d+)"), timeout=20)

    # Protocol Mode defaults to Report Protocol Mode (1).
    dut.write("p")
    dut.expect_exact("READ_REQUESTED", timeout=10)
    read = dut.expect(re.compile(rb"PROTOCOL_MODE_READ success=(\d+) mode=(\d+) context=(\w+)"), timeout=20)
    assert read.group(1) == b"1", "Protocol Mode read must succeed"
    assert read.group(2) == b"1", "Protocol Mode defaults to Report Protocol Mode"

    # Subscribe to the Boot Keyboard Input Report before switching modes.
    dut.write("B")
    dut.expect_exact("BOOT_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("BOOT_SUBSCRIBED success=1", timeout=20)

    # Switch the device to Boot Protocol Mode (Write Without Response).
    dut.write("b")
    dut.expect_exact("PROTOCOL_MODE_WRITE_REQUESTED", timeout=10)
    dut.expect_exact("PROTOCOL_MODE_WRITTEN success=1", timeout=20)
    mode = device.expect(re.compile(rb"PROTOCOL_MODE mode=(\d+) id=(\d+) context=(\w+)"), timeout=20)
    assert mode.group(1) == b"0", "device must observe Boot Protocol Mode (0)"
    assert int(mode.group(2)) != 0, "protocol mode event must carry a connection id"
    assert mode.group(3) == b"loop", "onProtocolMode must fire from update()/loop"

    # A keyboard report now travels over the Boot Keyboard Input Report.
    device.write("k")
    device.expect_exact("INPUT_SENT", timeout=10)
    boot = dut.expect(re.compile(
        rb"BOOT_INPUT length=(\d+) modifiers=(\d+) key=(\d+) context=(\w+)"), timeout=20)
    assert int(boot.group(1)) == 8, "boot keyboard report is always 8 bytes"
    assert int(boot.group(2)) == 2, "Left Shift modifier bit (0x02)"
    assert int(boot.group(3)) == 4, "keycode for 'a' (0x04)"

    # Boot Keyboard Output Report drives the LED state on the device.
    dut.write("o")
    dut.expect_exact("BOOT_OUTPUT_WRITE_REQUESTED", timeout=10)
    dut.expect_exact("BOOT_OUTPUT_WRITTEN success=1", timeout=20)
    output = device.expect(re.compile(rb"OUTPUT_REPORT leds=(\d+) caps=(\d+) context=(\w+)"), timeout=20)
    assert int(output.group(1)) == 2, "Caps Lock LED bit (0x02)"
    assert output.group(2) == b"1", "capsLock() must be set"
    assert output.group(3) == b"loop", "onOutputReport must fire from update()/loop"

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
