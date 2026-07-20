import re
import time

HOST_MIDI_PATTERN = re.compile(
    rb"HOST_MIDI count=(\d+) "
    rb"m0_status=(\d+) m0_d1=(\d+) m0_d2=(\d+) m0_ts=(\d+) "
    rb"m1_status=(\d+) m1_d1=(\d+) m1_d2=(\d+) m1_ts=(\d+) context=(\w+)"
)
HOST_IN_PATTERN = re.compile(rb"HOST_IN count=(\d+) length=(\d+) b2=(\d+) b3=(\d+) b4=(\d+)")
READY_PATTERN = re.compile(rb"HOST_READY (\d)")


def _wait_ready(dut, attempts=20):
    for _ in range(attempts):
        dut.write("r")
        match = dut.expect(READY_PATTERN, timeout=10)
        if match.group(1) == b"1":
            return True
        time.sleep(0.5)
    return False


def test_midi_host_decodes_running_status(dut, peers):
    """EspBleMidiHost must discover/subscribe to a BLE MIDI peripheral and decode
    a running-status packet into two messages with correct timestamps, and its
    outgoing notes must reach the peripheral. The peer_device is a bundled-NimBLE
    peripheral, so decoding is validated against an independent sender."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("HOST_SCAN_STARTED", timeout=10)
    dut.expect(re.compile(rb"HOST_CONNECTED id=(\d+)"), timeout=20)
    assert _wait_ready(dut), "host did not finish MIDI discovery/subscription"

    dut.write("c")
    dut.expect_exact("HOST_MIDI_RESET", timeout=10)

    # The peripheral notifies two Note On messages using running status.
    device.write("m")
    device.expect_exact("NOTIFIED", timeout=10)
    time.sleep(0.5)
    dut.write("q")
    match = dut.expect(HOST_MIDI_PATTERN, timeout=10)
    count = int(match.group(1))
    assert count == 2, f"expected 2 decoded messages, got {count}"
    # First message: full status 0x90, note 60, velocity 64, timestamp 165.
    assert int(match.group(2)) == 0x90
    assert int(match.group(3)) == 60 and int(match.group(4)) == 64
    assert int(match.group(5)) == 165
    # Second message: running status reuses 0x90, note 64, velocity 64, ts 166.
    assert int(match.group(6)) == 0x90, "running status not inherited"
    assert int(match.group(7)) == 64 and int(match.group(8)) == 64
    assert int(match.group(9)) == 166
    assert match.group(10).decode() == "loop", "MIDI must be delivered from update()/loop"

    # Host -> device note reaches the peripheral.
    dut.write("n")
    dut.expect_exact("HOST_NOTE_SENT 1", timeout=10)
    time.sleep(0.5)
    device.write("q")
    received = device.expect(HOST_IN_PATTERN, timeout=10)
    assert int(received.group(1)) >= 1, "peripheral did not receive the host note"
    assert int(received.group(2)) == 5, "unexpected packet length"
    assert int(received.group(3)) == 0x90, "expected Note On status byte"
    assert int(received.group(4)) == 64 and int(received.group(5)) == 100

    dut.write("d")
    dut.expect_exact("HOST_DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"HOST_DISCONNECTED id=(\d+)"), timeout=20)
