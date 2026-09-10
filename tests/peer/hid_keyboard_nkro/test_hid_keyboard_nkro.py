import re
import time

NKRO_STATE_PATTERN = re.compile(
    rb"HOST_NKRO_STATE count=(\d+) high=(\d+) b=(\d+) b_released=(\d+)\r?\n"
)
CONNECTED_PATTERN = re.compile(rb"HOST_CONNECTED id=(\d+)\r?\n")
LED_STATE_PATTERN = re.compile(
    rb"DEVICE_LED_STATE leds=(\d+) num=(\d+) caps=(\d+) scroll=(\d+) connection=(\d+)\r?\n"
)


def connect(dut, device):
    """Bring up the HID link, from cleared bonds through discovery.

    Every test in this module establishes its own link. The cleanup fixture
    returns both boards to their boot state after each test, and a test has to
    pass when it is the only one selected, so nothing may be inherited from the
    test before it. The connection id is matched by pattern rather than by value
    because ids keep counting from boot: only a module's first connection is 1.
    """
    dut.write("x")
    device.write("x")
    dut.expect_exact("HOST_BONDS_CLEARED success=1", timeout=10)
    device.expect_exact("DEVICE_BONDS_CLEARED success=1", timeout=10)

    dut.write("s")
    dut.expect_exact("HOST_SCAN_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_CONNECT_STARTED success=1", timeout=20)
    dut.expect(CONNECTED_PATTERN, timeout=20)
    dut.expect_exact("HOST_DISCOVERY_STARTED success=1", timeout=20)
    dut.expect_exact("HOST_DISCOVERED success=1 report=1 output=1 detail=", timeout=20)


def _keyboard_and_led_state(dut, device):

    device.write("n")
    device.expect_exact("DEVICE_NKRO_SENT success=1", timeout=20)
    dut.expect_exact("HOST_NKRO_STATE count=8 high=1 b=1 b_released=0", timeout=20)

    device.write("b")
    device.expect_exact("DEVICE_RELEASE_USAGE success=1", timeout=10)
    dut.expect_exact("HOST_NKRO_STATE count=7 high=1 b=0 b_released=1", timeout=20)

    dut.write("l")
    dut.expect_exact("HOST_LEDS_WRITTEN success=1", timeout=10)
    device.expect_exact("DEVICE_OUTPUT leds=3", timeout=20)
    # ledState() answers "what is it now?" without the sketch caching the
    # callback: the same value the host wrote (Num Lock + Caps Lock = 0x03),
    # attributed to the connection it came from. Which number that is depends on
    # how many links this board has made, so only "a real one" is asserted.
    device.write("e")
    match = device.expect(LED_STATE_PATTERN, timeout=10)
    assert match.groups()[:4] == (b"3", b"1", b"1", b"0"), (
        f"expected leds=3 num=1 caps=1 scroll=0, got {match.group(0).decode()}"
    )
    assert match.group(5) != b"0", "the LED state must name the connection it came from"

    device.write("r")
    device.expect_exact("DEVICE_RELEASE_ALL success=1", timeout=10)
    dut.expect_exact("HOST_NKRO_STATE count=0 high=0 b=0 b_released=0", timeout=20)


def _whole_state_is_one_report(dut, device):
    """`sendReport(EspBleHidKeyboardNkroReport)` puts the whole NKRO state into a
    single notification. The `keys[6]` overload cannot: it carries six usages even
    with NKRO enabled, and the incremental `pressUsage()` path emits one
    notification per key, so the host would observe the chord building up one key
    at a time and paced by the connection interval.

    Proof that it was one report: the *first* state event the host sees already
    holds all nine usages. Eight are in the 0x00-0xDF bitmap; LeftShift (0xE1) is
    above it, so `press()` routes it into `modifiers` — the host bitmap carries
    modifier usages too, hence a count of nine.
    """

    device.write("w")
    device.expect_exact(
        "DEVICE_NKRO_STATE_SENT success=1 represented=1 modifiers=2", timeout=20
    )
    match = dut.expect(NKRO_STATE_PATTERN, timeout=20)
    assert match.group(1) == b"9", (
        "the first state event must already hold every key of the report "
        f"(count={match.group(1).decode()})"
    )
    assert match.group(2) == b"1", "the high usage 0x87 must be down"
    assert match.group(3) == b"1", "usage 0x05 must be down"

    # Everything this report put down goes away together. Unlike the first test,
    # usage 0x05 is still held here, so releaseAll() is what releases it.
    device.write("r")
    device.expect_exact("DEVICE_RELEASE_ALL success=1", timeout=10)
    dut.expect_exact("HOST_NKRO_STATE count=0 high=0 b=0 b_released=1", timeout=20)


def _held_state_tracks_what_the_host_was_told(dut, device):
    """`heldState()` is the NKRO state the host was last told about, so a caller
    that rebuilds the whole state each cycle can compare against it instead of
    keeping a shadow copy — the library deliberately does not suppress duplicate
    reports itself, because after a `releaseAll()` it cannot know what the host
    still holds.

    It must reflect every path that sends: the whole-state overload, the
    incremental `releaseUsage()`, and `releaseAll()`.
    """

    device.write("w")
    device.expect_exact(
        "DEVICE_NKRO_STATE_SENT success=1 represented=1 modifiers=2", timeout=20
    )
    dut.expect(NKRO_STATE_PATTERN, timeout=20)

    # Eight bitmap usages plus LeftShift, which lives in `modifiers`.
    device.write("h")
    device.expect_exact(
        "DEVICE_HELD count=9 a=1 high=1 shift=1 modifiers=2", timeout=10
    )

    # An incremental release must move the held state too, not just the wire.
    device.write("b")
    device.expect_exact("DEVICE_RELEASE_USAGE success=1", timeout=10)
    dut.expect(NKRO_STATE_PATTERN, timeout=20)
    device.write("h")
    device.expect_exact(
        "DEVICE_HELD count=8 a=1 high=1 shift=1 modifiers=2", timeout=10
    )

    device.write("r")
    device.expect_exact("DEVICE_RELEASE_ALL success=1", timeout=10)
    dut.expect(NKRO_STATE_PATTERN, timeout=20)
    device.write("h")
    device.expect_exact(
        "DEVICE_HELD count=0 a=0 high=0 shift=0 modifiers=0", timeout=10
    )


def _led_state_follows_the_host_without_a_callback(dut, device):
    """`ledState()` must track the host whether or not `onOutputReport()` is
    installed. Without a callback, `dispatchPendingOutputReports()` returns early
    and nothing drains the output queue, so the queue sits full and every later
    report is dropped — the saved state therefore has to be updated before the
    queue rather than at dispatch time.

    The flood writes ten LED values while the queue holds eight, so reports are
    definitely dropped, and ends on a value distinct from the earlier test's.
    """

    device.write("u")
    device.expect_exact("DEVICE_OUTPUT_CALLBACK installed=0", timeout=10)

    dut.write("L")
    dut.expect_exact("HOST_LEDS_FLOOD sent=10", timeout=20)
    time.sleep(1)

    # Num + Caps + Scroll = 0x07, the last value written. The connection is this
    # test's own, so its number is whatever the device has reached by now.
    device.write("e")
    match = device.expect(LED_STATE_PATTERN, timeout=10)
    assert match.groups()[:4] == (b"7", b"1", b"1", b"1"), (
        f"expected leds=7 num=1 caps=1 scroll=1, got {match.group(0).decode()}"
    )
    assert match.group(5) != b"0", "the LED state must name the connection it came from"

    # Put it back: the next case may be the one that needs the callback.
    device.write("U")
    device.expect_exact("DEVICE_OUTPUT_CALLBACK installed=1", timeout=10)



def test_hid_keyboard_nkro(dut, peers, run_checks):
    """The NKRO cases, behind one link.

    The link is brought up once here rather than per case: nothing between the
    cases depends on it being new, and every case leaves the device as it found
    it. They are called from a list so that `ESPBLE_REVERSE_CHECKS=1` can prove
    the order is not load-bearing.
    """
    device = peers["device"]
    connect(dut, device)
    run_checks([
        _keyboard_and_led_state,
        _whole_state_is_one_report,
        _held_state_tracks_what_the_host_was_told,
        _led_state_follows_the_host_without_a_callback,
    ], dut, device)
