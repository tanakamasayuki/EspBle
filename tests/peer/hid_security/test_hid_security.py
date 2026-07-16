import re
import time

READ_PATTERN = re.compile(rb"HOST_READ success=(\d+) len=(\d+)")
QUERY_PATTERN = re.compile(rb"HOST_QUERY notifications=(\d+) connections=(\d+)")
INPUT_SENT_PATTERN = re.compile(rb"DEVICE_INPUT_SENT success=(\d+) error=(\w+)")


def test_hid_data_requires_encryption(dut, peers):
    """A security-enabled HID Keyboard Device must not expose HID Service
    data (Report Map, reports) to a Central over an unencrypted link."""
    device = peers["device"]

    device.write("x")
    device.expect_exact("DEVICE_BONDS_CLEARED success=1 count=0", timeout=10)
    device.write("?")
    device.expect_exact("DEVICE_ADVERTISING 1", timeout=20)
    dut.write("q")
    dut.expect(QUERY_PATTERN, timeout=10)

    dut.write("s")
    dut.expect_exact("HOST_SCAN_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_CONNECT_STARTED success=1", timeout=20)
    dut.expect(re.compile(rb"HOST_CONNECTED id=(\d+)"), timeout=20)
    device.expect(re.compile(rb"DEVICE_CONNECTED id=(\d+)"), timeout=20)

    # Reading the Report Map over the unencrypted link must return no data.
    dut.write("r")
    dut.expect_exact("HOST_READ_STARTED success=1", timeout=10)
    match = dut.expect(READ_PATTERN, timeout=20)
    assert match.group(2) == b"0", (
        "Report Map was readable over an unencrypted link "
        f"({int(match.group(2))} bytes)"
    )

    # HID discovery must fail without encryption.
    dut.write("i")
    dut.expect_exact("HOST_DISCOVERY_STARTED success=1", timeout=10)
    match = dut.expect(re.compile(rb"HOST_DISCOVERED success=(\d+)"), timeout=20)
    assert match.group(1) == b"0", "HID discovery succeeded over an unencrypted link"

    # Even if the Central manages to subscribe, input reports must not be
    # pushed over the unencrypted link.
    dut.write("v")
    dut.expect_exact("HOST_SUBSCRIBE_STARTED success=1", timeout=10)
    dut.expect(re.compile(rb"HOST_SUBSCRIBED success=(\d+)"), timeout=20)
    time.sleep(0.5)
    device.write("k")
    match = device.expect(INPUT_SENT_PATTERN, timeout=10)
    assert match.group(1) == b"0", (
        "input report was sent over an unencrypted link"
    )
    time.sleep(0.5)
    dut.write("q")
    match = dut.expect(QUERY_PATTERN, timeout=10)
    assert match.group(1) == b"0", (
        "an input report notification arrived over an unencrypted link"
    )

    dut.write("d")
    dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
    dut.expect(re.compile(rb"HOST_DISCONNECTED id=(\d+)"), timeout=20)
    device.expect_exact("DEVICE_READVERTISING 1", timeout=20)
    device.write("x")
    device.expect_exact("DEVICE_BONDS_CLEARED success=1 count=0", timeout=10)
