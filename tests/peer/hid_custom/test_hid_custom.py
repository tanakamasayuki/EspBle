import re


def test_hid_custom(dut, peers):
    """Validate a Custom HID device built with an arbitrary Report Descriptor via
    ble.hidCustom(). The device composes a vendor-defined report map (Report ID 1
    with a 2-byte input and a 1-byte output) into the HID service. A generic GATT
    client reads the Report Map (0x2A4B) and checks its 34-byte length and the
    vendor-defined usage-page signature, subscribes to the input Report (0x2A4D),
    and decodes a custom 2-byte input report (dial delta +5, buttons 0x01)."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1 maplen=34", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect(re.compile(rb"CONNECTED id=(\d+)"), timeout=20)
    device.expect(re.compile(rb"HID_CONNECTED id=(\d+)"), timeout=20)

    # Read and verify the arbitrary Report Map.
    dut.write("m")
    dut.expect_exact("READ_REQUESTED", timeout=10)
    report_map = dut.expect(re.compile(
        rb"REPORT_MAP success=(\d+) length=(\d+) sig=(\d+),(\d+),(\d+) context=(\w+)"), timeout=20)
    assert report_map.group(1) == b"1", "Report Map read must succeed"
    assert int(report_map.group(2)) == 34, "Report Map must be the full custom descriptor"
    # Vendor-Defined usage page 0xFF00: 0x06, 0x00, 0xFF.
    assert int(report_map.group(3)) == 6, "descriptor starts with Usage Page (0x06)"
    assert int(report_map.group(4)) == 0, "usage page low byte 0x00"
    assert int(report_map.group(5)) == 255, "usage page high byte 0xFF"

    # Subscribe to the custom input report and receive a notification.
    dut.write("S")
    dut.expect_exact("SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("INPUT_SUBSCRIBED success=1", timeout=20)

    device.write("i")
    device.expect_exact("INPUT_SENT", timeout=10)
    report = dut.expect(re.compile(
        rb"INPUT_REPORT length=(\d+) delta=(-?\d+) buttons=(\d+) context=(\w+)"), timeout=20)
    assert int(report.group(1)) == 2, "custom input report is 2 bytes"
    assert int(report.group(2)) == 5, "dial delta should decode to +5"
    assert int(report.group(3)) == 1, "buttons bitfield should be 0x01"
    assert report.group(4) == b"loop", "notification must be delivered from update()/loop"

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
