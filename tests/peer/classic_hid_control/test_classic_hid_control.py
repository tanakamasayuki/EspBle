import re


ADDRESS = rb"([0-9a-f]{2}(?::[0-9a-f]{2}){5})"


def test_get_report_set_report_and_protocol_mode(dut, peers):
    """The HID control channel, which real Hosts use and EspBle did not answer.

    Every exchange here is a round trip between two boards, so a request that is
    accepted locally but never answered shows up as a missing event rather than
    as a passing test. The device side is the one under test for Get_Report and
    Set_Report; the host side is the one under test for the requests.
    """
    peer = peers["device"]
    peer.expect_exact("CONTROL_HOST_READY", timeout=30)
    ready = dut.expect(
        re.compile(rb"CONTROL_DEVICE_READY address=" + ADDRESS), timeout=30)
    address = ready.group(1).decode()

    peer.write("c" + address + "\n")
    peer.expect_exact("CONTROL_HOST_CONNECT requested=1", timeout=20)
    peer.expect(re.compile(rb"CONTROL_HOST_CONNECTED peer=" + ADDRESS), timeout=40)
    dut.expect(re.compile(rb"CONTROL_DEVICE_CONNECTED peer=" + ADDRESS), timeout=30)

    # Get_Report: the Host asks and the device answers from its own state. The
    # value has to arrive as data, not as an empty success, and it carries the
    # report ID in front of the payload as every raw report here does.
    peer.write("g\n")
    peer.expect_exact("CONTROL_HOST_GET requested=1", timeout=10)
    dut.expect_exact("CONTROL_DEVICE_GET type=1 id=1 max=8", timeout=20)
    dut.expect_exact("CONTROL_DEVICE_ANSWERED sent=1", timeout=10)
    peer.expect_exact("CONTROL_HOST_REPORT success=1 hex=0110203040", timeout=20)

    # Answering when nothing was asked would put an unsolicited report on the
    # control channel, so the device refuses its own caller.
    dut.write("w")
    dut.expect_exact("CONTROL_DEVICE_STALE sent=0 error=InvalidState", timeout=10)

    # A refusal is an answer: the Host learns the request failed instead of
    # waiting for its own timeout.
    dut.write("r")
    dut.expect_exact("CONTROL_DEVICE_MODE refuse", timeout=10)
    peer.write("G\n")
    peer.expect_exact("CONTROL_HOST_GET requested=1", timeout=10)
    dut.expect_exact("CONTROL_DEVICE_GET type=1 id=9 max=8", timeout=20)
    dut.expect_exact("CONTROL_DEVICE_REFUSED sent=1", timeout=10)
    peer.expect_exact("CONTROL_HOST_REPORT success=0 hex=", timeout=20)
    dut.write("a")
    dut.expect_exact("CONTROL_DEVICE_MODE answer", timeout=10)

    # Set_Report keeps the type the Host used. A Feature report only exists on
    # the control channel, so reporting it as an Output report would be wrong
    # and undetectable from the payload alone. The report ID arrives in its own
    # field, so the value is the payload alone.
    peer.write("s\n")
    peer.expect_exact("CONTROL_HOST_SET requested=1", timeout=10)
    dut.expect_exact("CONTROL_DEVICE_SET type=3 id=3 hex=abcd", timeout=20)
    peer.expect_exact("CONTROL_HOST_SENT success=1", timeout=20)

    # The protocol mode belongs to the Host; the device can only observe it.
    peer.write("b\n")
    peer.expect_exact("CONTROL_HOST_SET_PROTOCOL requested=1", timeout=10)
    peer.expect_exact("CONTROL_HOST_PROTOCOL success=1 mode=1", timeout=20)
    dut.expect_exact("CONTROL_DEVICE_PROTOCOL mode=1", timeout=20)
    dut.write("?")
    dut.expect(
        re.compile(rb"CONTROL_DEVICE_STATE connected=1 protocol=1 dropped=0"),
        timeout=10,
    )

    # Reading it back reports the mode in effect, not the one last requested.
    peer.write("p\n")
    peer.expect_exact("CONTROL_HOST_GET_PROTOCOL requested=1", timeout=10)
    peer.expect_exact("CONTROL_HOST_PROTOCOL success=1 mode=1", timeout=20)

    peer.write("B\n")
    peer.expect_exact("CONTROL_HOST_SET_PROTOCOL requested=1", timeout=10)
    peer.expect_exact("CONTROL_HOST_PROTOCOL success=1 mode=0", timeout=20)
    dut.expect_exact("CONTROL_DEVICE_PROTOCOL mode=0", timeout=20)

    # The idle rate is how often the device repeats an unchanged report. Zero
    # means "only on change", which is what this device does.
    peer.write("i\n")
    peer.expect_exact("CONTROL_HOST_SET_IDLE requested=1", timeout=10)
    peer.expect_exact("CONTROL_HOST_IDLE success=1 rate=0", timeout=20)
    peer.write("I\n")
    peer.expect_exact("CONTROL_HOST_GET_IDLE requested=1", timeout=10)
    peer.expect_exact("CONTROL_HOST_IDLE success=1 rate=0", timeout=20)

    # Get_Report still works after all of the above, so none of the control
    # exchanges left the channel in a state the next request cannot use.
    peer.write("g\n")
    peer.expect_exact("CONTROL_HOST_GET requested=1", timeout=10)
    peer.expect_exact("CONTROL_HOST_REPORT success=1 hex=0110203040", timeout=20)

    # A virtual cable unplug ends the connection on both sides, which is what
    # makes it different from a disconnect: the peer is meant to forget it.
    peer.write("u\n")
    peer.expect_exact("CONTROL_HOST_UNPLUG requested=1", timeout=10)
    peer.expect_exact("CONTROL_HOST_DISCONNECTED", timeout=30)
    dut.expect_exact("CONTROL_DEVICE_DISCONNECTED", timeout=30)
    dut.write("?")
    dut.expect(
        re.compile(rb"CONTROL_DEVICE_STATE connected=0 protocol=\d dropped=0"),
        timeout=10,
    )
