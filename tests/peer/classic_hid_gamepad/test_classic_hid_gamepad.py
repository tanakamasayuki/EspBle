import re


ADDRESS = rb"([0-9a-f]{2}(?::[0-9a-f]{2}){5})"


def test_classic_gamepad_reports_reach_the_host(dut, peers):
    """The gamepad is the profile Classic exists for.

    An older console accepts BR/EDR HID only, so BLE is not an alternative. The
    report is not decoded into events by this library, which means the raw bytes
    are the contract: signed axes stay signed, the hat is the direction that was
    sent, and the buttons are a little-endian bit field.
    """
    peer = peers["device"]
    peer.expect_exact("GAMEPAD_HOST_READY", timeout=30)
    ready = dut.expect(
        re.compile(rb"GAMEPAD_DEVICE_READY address=" + ADDRESS + rb" gamepad=1"),
        timeout=30,
    )
    address = ready.group(1).decode()

    peer.write("c" + address + "\n")
    peer.expect_exact("GAMEPAD_HOST_CONNECT requested=1", timeout=20)
    peer.expect(re.compile(rb"GAMEPAD_HOST_CONNECTED peer=" + ADDRESS), timeout=40)
    dut.expect(re.compile(rb"GAMEPAD_DEVICE_CONNECTED peer=" + ADDRESS), timeout=30)

    # x=96 (0x60), y=-96 (0xa0), four centred axes, hat=1 (up), buttons 1 and 2
    # as 0x00000003 little-endian. The leading 0x03 is the gamepad report ID.
    dut.write("g")
    dut.expect_exact("GAMEPAD_DEVICE_SENT sent=1", timeout=10)
    peer.expect_exact(
        "GAMEPAD_HOST_RAW id=3 len=12 hex=0360a0000000000103000000", timeout=20)

    # Releasing centres every axis and clears the buttons; a Host holds the last
    # report until then, so this is what stops the input.
    dut.write("r")
    dut.expect_exact("GAMEPAD_DEVICE_RELEASED sent=1", timeout=10)
    peer.expect_exact(
        "GAMEPAD_HOST_RAW id=3 len=12 hex=030000000000000000000000", timeout=20)

    # The keyboard shares the same device record, so its report keeps report ID 1
    # rather than being folded into the gamepad's.
    dut.write("k")
    dut.expect_exact("GAMEPAD_DEVICE_KEY sent=1", timeout=10)
    peer.expect(
        re.compile(rb"GAMEPAD_HOST_RAW id=1 len=9 hex=0100000400000000"), timeout=20)

    dut.write("?")
    dut.expect_exact("GAMEPAD_DEVICE_STATE connected=1", timeout=10)
