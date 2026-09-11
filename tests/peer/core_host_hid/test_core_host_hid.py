import re


def test_hid_host_interoperates_with_a_core_bluedroid_keyboard(dut, peers, probe):
    """EspBle's HID Host against a HOGP keyboard built on the core's stack.

    The peer links no EspBle code: its Report Map, report characteristics and
    LED output report come from `BLEHIDDevice` on Bluedroid. Two EspBle boards
    share one descriptor generator and one parser, so they cannot catch a
    descriptor the rest of the world writes differently; this pairing can.
    """
    peer = peers["device"]

    probe(peer, "?\n", re.compile(rb"HIDPEER_READY address=[0-9a-f:]+"))
    probe(dut, "?\n", re.compile(rb"HIDHOST_READY"))

    dut.write("s\n")
    dut.expect_exact("HIDHOST_SCAN started=1", timeout=10)
    dut.expect(re.compile(rb"HIDHOST_CONNECT requested=1 peer=[0-9a-f:]+"), timeout=30)
    peer.expect(re.compile(rb"HIDPEER_CONNECTED id=\d+"), timeout=30)

    # HOGP keeps the Report Map behind encryption, so discovery can only follow
    # a successful pairing across the two stacks.
    dut.expect(
        re.compile(rb"HIDHOST_SECURITY success=1 encrypted=1 bonded=\d"), timeout=40
    )
    peer.expect_exact("HIDPEER_AUTH success=1", timeout=40)
    dut.expect_exact("HIDHOST_DISCOVER_REQUESTED 1", timeout=20)

    # The descriptor has no report ID, so the host must report 0 rather than
    # inventing one, and it must see the output report and the battery service
    # the Bluedroid device published.
    discovered = dut.expect(
        re.compile(
            rb"HIDHOST_DISCOVERED success=1 report_id=(\d+) output=(\d) battery=(\d) level=(\d+)"
        ),
        timeout=30,
    )
    assert discovered.group(1) == b"0", "a descriptor without report IDs must report 0"
    assert discovered.group(2) == b"1", "the LED output report was not discovered"
    assert discovered.group(3) == b"1", "the battery level was not discovered"
    assert discovered.group(4) == b"77", "battery level did not survive the crossing"

    # A key press decoded from a report the other stack packed.
    peer.write("a\n")
    peer.expect_exact("HIDPEER_SENT modifiers=00 usage=04", timeout=10)
    dut.expect_exact("HIDHOST_KEY usage=04 ascii=a modifiers=00 pressed=1", timeout=20)

    peer.write("z\n")
    peer.expect_exact("HIDPEER_SENT modifiers=00 usage=00", timeout=10)
    dut.expect_exact("HIDHOST_KEY usage=04 ascii=a modifiers=00 pressed=0", timeout=20)

    # The modifier has to reach the layout conversion, not just the raw report.
    peer.write("A\n")
    peer.expect_exact("HIDPEER_SENT modifiers=02 usage=04", timeout=10)
    dut.expect_exact("HIDHOST_KEY usage=04 ascii=A modifiers=02 pressed=1", timeout=20)
    peer.write("z\n")
    dut.expect(re.compile(rb"HIDHOST_KEY usage=04 ascii=\S modifiers=\d\d pressed=0"), timeout=20)

    # LED output runs host to device: the write must land on the peer's output
    # report with the bit the host was asked to set.
    dut.write("l\n")
    dut.expect_exact("HIDHOST_LED_REQUESTED 1", timeout=10)
    peer.expect(re.compile(rb"HIDPEER_LED count=\d+ value=01"), timeout=20)
    dut.write("L\n")
    dut.expect_exact("HIDHOST_LED_REQUESTED 1", timeout=10)
    peer.expect(re.compile(rb"HIDPEER_LED count=\d+ value=00"), timeout=20)

    # The device drops the link; the host must report it rather than keep a
    # stale connection with a stale discovery.
    peer.write("d\n")
    peer.expect_exact("HIDPEER_DISCONNECT requested=1", timeout=10)
    dut.expect(re.compile(rb"HIDHOST_DISCONNECTED reason=\d+"), timeout=20)
    state = probe(
        dut, "?\n", re.compile(rb"HIDHOST_STATE connected=(\d) discovered=(\d)")
    )
    assert state.group(1) == b"0" and state.group(2) == b"0"
