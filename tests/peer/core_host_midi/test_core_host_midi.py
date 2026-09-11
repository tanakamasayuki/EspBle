import re


def test_ble_midi_interoperates_with_a_core_bluedroid_device(dut, peers, probe):
    """EspBle's MIDI Host against a MIDI service built on the core's stack.

    The peer links no EspBle code: it assembles the MIDI service by hand on the
    BLE wrapper Arduino-ESP32 ships and frames the packets itself. EspBle's
    encoder and decoder are therefore not talking to each other, which is the
    only way a framing assumption they share can be caught.
    """
    peer = peers["device"]

    probe(peer, "?\n", re.compile(rb"MIDIPEER_READY address=[0-9a-f:]+"))
    probe(dut, "?\n", re.compile(rb"MIDIHOST_READY"))

    dut.write("s\n")
    dut.expect_exact("MIDIHOST_SCAN started=1", timeout=10)
    dut.expect(re.compile(rb"MIDIHOST_CONNECT requested=1 peer=[0-9a-f:]+"), timeout=30)
    peer.expect(re.compile(rb"MIDIPEER_CONNECTED id=\d+"), timeout=30)
    dut.expect_exact("MIDIHOST_DISCOVER_REQUESTED 1", timeout=20)

    # Subscribing is what makes the peer able to send at all, and the peer
    # reports the CCCD bits it actually received.
    peer.expect_exact("MIDIPEER_CCCD value=0001", timeout=30)
    # ready() turns true only after discovery and the subscription complete, so
    # ask until it does rather than reading the state once.
    probe(dut, "?\n", re.compile(rb"MIDIHOST_STATE connected=1 ready=1"))

    dut.write("z\n")
    dut.expect_exact("MIDIHOST_COUNTERS_RESET", timeout=10)

    # A single Note On, framed with the header and timestamp bytes the spec
    # requires. The 13-bit timestamp is assembled from both.
    peer.write("n\n")
    peer.expect_exact("MIDIPEER_TX hex=80a1903c64", timeout=10)
    message = dut.expect(
        re.compile(rb"MIDIHOST_MESSAGE index=1 status=90 data1=3c data2=64 ts=(\d+)"),
        timeout=20,
    )
    assert int(message.group(1)) == 0x21, "timestamp low bits were not decoded"

    # Running status: the second note in the packet carries no status byte, so a
    # decoder that does not track it drops or mangles the message.
    peer.write("r\n")
    peer.expect_exact("MIDIPEER_TX hex=80a2903c64a34050", timeout=10)
    dut.expect(
        re.compile(rb"MIDIHOST_MESSAGE index=2 status=90 data1=3c data2=64 ts=\d+"),
        timeout=20,
    )
    dut.expect(
        re.compile(rb"MIDIHOST_MESSAGE index=3 status=90 data1=40 data2=50 ts=\d+"),
        timeout=20,
    )

    # System Exclusive with the timestamp byte before F7, which is where a
    # decoder that treats the stream as plain MIDI goes wrong.
    # SysEx is delivered as chunks: the payload up to the timestamp byte, then
    # the terminator. Both flags are asserted so a decoder that merges or drops
    # a chunk fails here.
    peer.write("s\n")
    peer.expect_exact("MIDIPEER_TX hex=80a4f07d010203a5f7", timeout=10)
    sysex = dut.expect(
        re.compile(rb"MIDIHOST_SYSEX start=1 end=0 length=(\d+) first=([0-9a-f]{2})"),
        timeout=20,
    )
    assert sysex.group(2) == b"7d", "SysEx payload did not start at the manufacturer id"
    assert int(sysex.group(1)) == 4, "SysEx payload length changed across the stacks"
    dut.expect(re.compile(rb"MIDIHOST_SYSEX start=0 end=1 length=\d+"), timeout=20)

    # The reverse direction: EspBle frames the packet and the peer prints the
    # bytes it received, so the header and timestamp EspBle produced are checked
    # by an implementation that did not write them.
    dut.write("n\n")
    dut.expect_exact("MIDIHOST_NOTE_SENT 1", timeout=10)
    received = peer.expect(
        re.compile(rb"MIDIPEER_RX count=1 length=(\d+) hex=([0-9a-f]+)"), timeout=20
    )
    payload = received.group(2).decode()
    assert int(received.group(1)) == 5, "a single Note On must be five bytes"
    assert payload[4:] == "904055", f"unexpected MIDI bytes in {payload}"
    assert int(payload[0:2], 16) & 0x80, "the header byte must have bit 7 set"
    assert int(payload[2:4], 16) & 0x80, "the timestamp byte must have bit 7 set"

    dut.write("c\n")
    dut.expect_exact("MIDIHOST_CC_SENT 1", timeout=10)
    control = peer.expect(
        re.compile(rb"MIDIPEER_RX count=2 length=5 hex=[0-9a-f]{4}([0-9a-f]{6})"),
        timeout=20,
    )
    assert control.group(1) == b"b1072a", "Control Change did not survive the crossing"

    peer.write("d\n")
    peer.expect_exact("MIDIPEER_DISCONNECT requested=1", timeout=10)
    dut.expect(re.compile(rb"MIDIHOST_DISCONNECTED reason=\d+"), timeout=20)
