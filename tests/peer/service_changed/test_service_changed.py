import re


def test_service_changed(dut, peers):
    """The server sends a GATT Service Changed indication via
    notifyServicesChanged(); the client, subscribed to the Generic Attribute
    Service Changed characteristic (0x1801/0x2A05), receives the indication and
    decodes the changed attribute-handle range (0x0001..0xFFFF = 1..65535)."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("SC_SUBSCRIBE_REQUESTED", timeout=20)
    dut.expect_exact("SC_SUBSCRIBED success=1", timeout=20)

    # Trigger the Service Changed indication once the client is subscribed.
    device.write("c")
    device.expect_exact("SERVICE_CHANGED sent=1", timeout=10)

    indication = dut.expect(re.compile(
        rb"SC_INDICATION valid=(\d+) indication=(\d+) start=(\d+) end=(\d+) context=(\w+)"), timeout=20)
    assert indication.group(1) == b"1", "Service Changed payload should be 4 bytes"
    assert indication.group(2) == b"1", "must arrive as an indication"
    assert int(indication.group(3)) == 1, "start handle should be 0x0001"
    assert int(indication.group(4)) == 65535, "end handle should be 0xFFFF"
    assert indication.group(5) == b"loop", "must be delivered from update()/loop"

    dut.write("u")
    dut.expect_exact("SC_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("SC_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
