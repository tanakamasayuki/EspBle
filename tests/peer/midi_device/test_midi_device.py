import re
import time

NOTIFY_PATTERN = re.compile(
    rb"NOTIFY count=(\d+) length=(\d+) b0=(\d+) b1=(\d+) b2=(\d+) b3=(\d+) b4=(\d+)"
)
DEVICE_IN_PATTERN = re.compile(
    rb"DEVICE_IN count=(\d+) status=(\d+) data1=(\d+) data2=(\d+) context=(\w+)"
)


def _notify(dut):
    dut.write("q")
    match = dut.expect(NOTIFY_PATTERN, timeout=10)
    return {
        "count": int(match.group(1)),
        "length": int(match.group(2)),
        "bytes": [int(match.group(i)) for i in range(3, 8)],
    }


def _device_in(device):
    device.write("q")
    match = device.expect(DEVICE_IN_PATTERN, timeout=10)
    return {
        "count": int(match.group(1)),
        "status": int(match.group(2)),
        "data1": int(match.group(3)),
        "data2": int(match.group(4)),
        "context": match.group(5).decode(),
    }


def test_midi_device_wire_format(dut, peers):
    """EspBleMidiDevice must notify spec-shaped BLE MIDI packets (header +
    timestamp + message) and decode host writes. The DUT is a bundled-NimBLE
    central, so the wire bytes are validated independently of EspBle."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    # Connect and subscribe from the independent central.
    dut.write("s")
    dut.expect_exact("MIDI_SCAN_STARTED", timeout=10)
    dut.expect_exact("MIDI_READ length=0", timeout=20)  # BLE MIDI read is empty
    dut.expect_exact("MIDI_READY 1", timeout=20)
    dut.write("c")
    dut.expect_exact("MIDI_COUNTERS_RESET", timeout=10)

    # Note On -> one notification carrying [header, timestamp, 0x90, note, vel].
    device.write("1")
    device.expect_exact("NOTE_ON_SENT 1", timeout=10)
    time.sleep(0.5)
    note_on = _notify(dut)
    assert note_on["count"] >= 1, "no notification received for Note On"
    assert note_on["length"] == 5, note_on
    header, timestamp, status, note, velocity = note_on["bytes"]
    assert header & 0x80 and not (header & 0x40), f"bad header byte 0x{header:02x}"
    assert timestamp & 0x80, f"bad timestamp byte 0x{timestamp:02x}"
    assert status == 0x90, f"expected Note On status, got 0x{status:02x}"
    assert note == 60 and velocity == 100, note_on

    # Note Off -> status 0x80, velocity 0.
    device.write("0")
    device.expect_exact("NOTE_OFF_SENT 1", timeout=10)
    time.sleep(0.5)
    note_off = _notify(dut)
    assert note_off["count"] >= 2, note_off
    assert note_off["bytes"][2] == 0x80, note_off
    assert note_off["bytes"][3] == 60 and note_off["bytes"][4] == 0, note_off

    # Host -> device: the central writes a raw Control Change packet.
    dut.write("w")
    dut.expect_exact("WRITE_DONE 1", timeout=10)
    time.sleep(0.5)
    received = _device_in(device)
    assert received["count"] >= 1, "device did not receive the host write"
    assert received["status"] == 0xB0, received
    assert received["data1"] == 7 and received["data2"] == 100, received
    assert received["context"] == "loop", "MIDI must be delivered from update()/loop"

    dut.write("d")
    dut.expect_exact("PEER_DISCONNECTED", timeout=10)
