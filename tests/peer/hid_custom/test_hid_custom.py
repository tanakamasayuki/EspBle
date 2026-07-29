import re


def test_hid_custom(dut, peers):
    """Validate a Custom HID device built with an arbitrary Report Descriptor via
    ble.hidCustom(), together with the handle-based GATT client operations. The
    device composes a vendor-defined report map (Report ID 1 with a 2-byte input,
    a 1-byte output and a 2-byte feature) into the HID service, so three Report
    characteristics share UUID 0x2A4D. A generic GATT client discovers the service,
    resolves each report by its distinct attribute handle, reads and length-checks
    the Report Map, subscribes to the input report BY HANDLE and decodes a custom
    2-byte report (dial delta +5, buttons 0x01), then writes the output report and
    the feature report BY HANDLE — which the device receives via onOutputReport
    and onFeatureReport."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1 maplen=47", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect(re.compile(rb"CONNECTED id=(\d+)"), timeout=20)
    device.expect(re.compile(rb"HID_CONNECTED id=(\d+)"), timeout=20)

    # Discovery resolves the three same-UUID Report characteristics to distinct
    # handles. Output and Feature are told apart by Write Without Response: only
    # the Output report carries it, because a Feature report is configuration and
    # is always written with a response.
    resolved = dut.expect(re.compile(
        rb"REPORTS_RESOLVED input=(\d+) output=(\d+) feature=(\d+) distinct=(\d+)"),
        timeout=20)
    input_handle = int(resolved.group(1))
    output_handle = int(resolved.group(2))
    feature_handle = int(resolved.group(3))
    assert resolved.group(4) == b"1", \
        "input, output and feature must resolve to distinct non-zero handles"
    assert len({input_handle, output_handle, feature_handle}) == 3

    # Read and length-check the arbitrary Report Map.
    dut.write("m")
    dut.expect_exact("READ_REQUESTED", timeout=10)
    report_map = dut.expect(re.compile(rb"REPORT_MAP success=(\d+) length=(\d+)"), timeout=20)
    assert report_map.group(1) == b"1", "Report Map read must succeed"
    assert int(report_map.group(2)) == 47, "Report Map must be the full custom descriptor"

    # Subscribe to the input report BY HANDLE and receive a notification.
    dut.write("S")
    dut.expect_exact("SUBSCRIBE_REQUESTED", timeout=10)
    subscribed = dut.expect(re.compile(
        rb"INPUT_SUBSCRIBED success=(\d+) handle=(\d+) context=(\w+)"), timeout=20)
    assert subscribed.group(1) == b"1", "subscribe by handle must succeed"
    assert int(subscribed.group(2)) == input_handle, "result handle must echo the input handle"

    device.write("i")
    device.expect_exact("INPUT_SENT", timeout=10)
    report = dut.expect(re.compile(
        rb"INPUT_REPORT handle=(\d+) delta=(-?\d+) buttons=(\d+) context=(\w+)"), timeout=20)
    assert int(report.group(1)) == input_handle, "notification handle must be the input handle"
    assert int(report.group(2)) == 5, "dial delta should decode to +5"
    assert int(report.group(3)) == 1, "buttons bitfield should be 0x01"
    assert report.group(4) == b"loop", "notification must be delivered from update()/loop"

    # Write the OUTPUT report BY HANDLE (the other 0x2A4D characteristic).
    dut.write("o")
    dut.expect_exact("OUTPUT_WRITE_REQUESTED", timeout=10)
    written = dut.expect(re.compile(
        rb"OUTPUT_WRITTEN success=(\d+) handle=(\d+) context=(\w+)"), timeout=20)
    assert written.group(1) == b"1", "write by handle must succeed"
    assert int(written.group(2)) == output_handle, "result handle must echo the output handle"

    output = device.expect(re.compile(
        rb"OUTPUT_REPORT id=(\d+) len=(\d+) byte0=(\d+) context=(\w+)"), timeout=20)
    assert int(output.group(1)) == 1, "output report id should be 1"
    assert int(output.group(2)) == 1, "output report is 1 byte"
    assert int(output.group(3)) == 2, "output byte should be the written LED value 0x02"

    # Write the FEATURE report BY HANDLE. addFeatureReport() gave it its own
    # 0x2A4D characteristic with a Report Reference of type Feature, so it is a
    # separate handle from the output report above.
    dut.write("f")
    dut.expect_exact("FEATURE_WRITE_REQUESTED", timeout=10)
    feature_written = dut.expect(re.compile(
        rb"FEATURE_WRITTEN success=(\d+) handle=(\d+) context=(\w+)"), timeout=20)
    assert feature_written.group(1) == b"1", "feature write by handle must succeed"
    assert int(feature_written.group(2)) == feature_handle, \
        "result handle must echo the feature handle"

    feature = device.expect(re.compile(
        rb"FEATURE_REPORT id=(\d+) len=(\d+) byte0=(\d+) byte1=(\d+) context=(\w+)"),
        timeout=20)
    assert int(feature.group(1)) == 1, "feature report id should be 1"
    assert int(feature.group(2)) == 2, "feature report is 2 bytes"
    assert int(feature.group(3)) == 0x5a, "first configuration byte"
    assert int(feature.group(4)) == 0xa5, "second configuration byte"
    assert feature.group(5) == b"loop", "feature report must be delivered from update()/loop"

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
