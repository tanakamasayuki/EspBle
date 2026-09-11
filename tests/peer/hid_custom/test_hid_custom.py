import re


def test_hid_custom(dut, peers):
    """Validate a Custom HID device built with an arbitrary Report Descriptor via
    ble.hidCustom(), together with the handle-based GATT client operations. The
    device composes a vendor-defined report map (Report ID 1 with a 2-byte input,
    a 1-byte output and a 2-byte feature) into the HID service, so three Report
    characteristics share UUID 0x2A4D. A generic GATT client discovers the service,
    reads each report's role out of its Report Reference descriptor (0x2908) BY
    HANDLE — the only way to name it, since all three descriptors are 0x2908 under
    characteristics that all are 0x2A4D — reads and length-checks the Report Map,
    subscribes to the input report BY HANDLE and decodes a custom 2-byte report
    (dial delta +5, buttons 0x01), then writes the output report and the feature
    report BY HANDLE — which the device receives via onOutputReport and
    onFeatureReport."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1 maplen=47", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect(re.compile(rb"CONNECTED id=(\d+)"), timeout=20)
    device.expect(re.compile(rb"HID_CONNECTED id=(\d+)"), timeout=20)

    # Discovery pairs each 0x2A4D Report characteristic with its own 0x2908 Report
    # Reference. The pairing is by owning value handle; the UUID pair cannot do it.
    dut.expect_exact("REPORTS_PAIRED count=3", timeout=20)
    pairs = {}
    for _ in range(3):
        pair = dut.expect(re.compile(
            rb"REPORT_PAIR char=(\d+) ref=(\d+) notify=(\d+) write=(\d+) wwr=(\d+)"),
            timeout=20)
        char_handle = int(pair.group(1))
        pairs[char_handle] = {
            "ref": int(pair.group(2)),
            "notify": pair.group(3) == b"1",
            "write": pair.group(4) == b"1",
            "wwr": pair.group(5) == b"1",
        }
    assert len(pairs) == 3, "the three Report characteristics must have distinct handles"
    assert all(entry["ref"] != 0 for entry in pairs.values()), \
        "every Report characteristic must expose a Report Reference descriptor"
    assert len({entry["ref"] for entry in pairs.values()}) == 3, \
        "each Report Reference must be its own attribute"

    # Read every Report Reference BY HANDLE and take the role from the type byte,
    # which is how HID declares it.
    dut.write("p")
    dut.expect_exact("REPORT_REF_REQUESTED count=3", timeout=10)
    roles = {}
    for _ in range(3):
        ref = dut.expect(re.compile(
            rb"REPORT_REF desc=(\d+) char=(\d+) id=(\d+) type=(\d+) context=(\w+)"),
            timeout=20)
        desc_handle = int(ref.group(1))
        char_handle = int(ref.group(2))
        assert int(ref.group(3)) == 1, "every report is under Report ID 1"
        assert ref.group(5) == b"loop", "results must be delivered from update()/loop"
        # The result must name the characteristic that owns the descriptor, and
        # that descriptor must be the one paired with it during discovery.
        assert char_handle in pairs, "result handle must be one of the Report characteristics"
        assert pairs[char_handle]["ref"] == desc_handle, \
            "descriptorHandle and handle must describe the same pairing as discovery"
        roles[int(ref.group(4))] = char_handle

    assert set(roles) == {1, 2, 3}, "one Input, one Output and one Feature report"

    resolved = dut.expect(re.compile(
        rb"REPORTS_RESOLVED input=(\d+) output=(\d+) feature=(\d+) distinct=(\d+)"),
        timeout=20)
    input_handle = int(resolved.group(1))
    output_handle = int(resolved.group(2))
    feature_handle = int(resolved.group(3))
    assert resolved.group(4) == b"1", \
        "input, output and feature must resolve to distinct non-zero handles"
    assert (input_handle, output_handle, feature_handle) == (roles[1], roles[2], roles[3])

    # The declared types must agree with the characteristic properties: an Input
    # report is notifiable, and only an Output report carries Write Without
    # Response — a Feature report is configuration, so it is always written with a
    # response.
    assert pairs[input_handle]["notify"], "the Input report must be notifiable"
    assert pairs[output_handle]["wwr"], "the Output report must allow write without response"
    assert pairs[feature_handle]["write"] and not pairs[feature_handle]["wwr"], \
        "the Feature report must be writable with a response only"

    # Error paths of the handle form: a zero handle is refused locally, and a
    # well-formed but absent handle is accepted then reported NotFound.
    dut.write("z")
    dut.expect_exact("REF_ZERO success=0 error=INVALID_ARGUMENT", timeout=10)
    dut.write("Z")
    dut.expect_exact("REF_BOGUS success=1", timeout=10)
    dut.expect_exact("REPORT_REF_FAILED desc=65520", timeout=20)

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
