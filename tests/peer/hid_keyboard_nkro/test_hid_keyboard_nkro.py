import re

NKRO_STATE_PATTERN = re.compile(
    rb"HOST_NKRO_STATE count=(\d+) high=(\d+) b=(\d+) b_released=(\d+)"
)


def test_hid_keyboard_nkro(dut, peers):
    device = peers["device"]
    dut.write("x")
    device.write("x")
    dut.expect_exact("HOST_BONDS_CLEARED success=1", timeout=10)
    device.expect_exact("DEVICE_BONDS_CLEARED success=1", timeout=10)

    dut.write("s")
    dut.expect_exact("HOST_SCAN_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_CONNECT_STARTED success=1", timeout=20)
    dut.expect_exact("HOST_CONNECTED id=1", timeout=20)
    dut.expect_exact("HOST_DISCOVERY_STARTED success=1", timeout=20)
    dut.expect_exact("HOST_DISCOVERED success=1 report=1 output=1 detail=", timeout=20)

    device.write("n")
    device.expect_exact("DEVICE_NKRO_SENT success=1", timeout=20)
    dut.expect_exact("HOST_NKRO_STATE count=8 high=1 b=1 b_released=0", timeout=20)

    device.write("b")
    device.expect_exact("DEVICE_RELEASE_USAGE success=1", timeout=10)
    dut.expect_exact("HOST_NKRO_STATE count=7 high=1 b=0 b_released=1", timeout=20)

    dut.write("l")
    dut.expect_exact("HOST_LEDS_WRITTEN success=1", timeout=10)
    device.expect_exact("DEVICE_OUTPUT leds=3", timeout=20)

    device.write("r")
    device.expect_exact("DEVICE_RELEASE_ALL success=1", timeout=10)
    dut.expect_exact("HOST_NKRO_STATE count=0 high=0 b=0 b_released=0", timeout=20)


def test_nkro_whole_state_is_one_report(dut, peers):
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
    device = peers["device"]

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


def test_held_state_tracks_what_the_host_was_told(dut, peers):
    """`heldState()` is the NKRO state the host was last told about, so a caller
    that rebuilds the whole state each cycle can compare against it instead of
    keeping a shadow copy — the library deliberately does not suppress duplicate
    reports itself, because after a `releaseAll()` it cannot know what the host
    still holds.

    It must reflect every path that sends: the whole-state overload, the
    incremental `releaseUsage()`, and `releaseAll()`.
    """
    device = peers["device"]

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


def test_nkro_requires_mtu_32(dut, peers):
    """An NKRO report is 29 bytes, so it needs an MTU of at least 32 (29 plus the
    3-byte ATT header). `begin()` refuses a lower `preferredMtu` up front instead of
    letting every report notify fail silently against the MTU payload guard later —
    a silent failure here looks like "the keyboard sends nothing", with no error to
    point at. The library also does not quietly raise the MTU behind the caller's
    back, because that would hide a configuration the application chose.

    The device sketch walks the boundary with end()/begin() cycles: the spec minimum
    (23), one below the limit (31), then the limit itself (32), and finally back to
    the configuration the rest of this suite runs with.
    """
    device = peers["device"]

    device.write("m")
    device.expect_exact(
        "DEVICE_MTU_23 success=0 error=INVALID_ARGUMENT "
        "detail=NKRO keyboard requires preferredMtu >= 32 "
        "(29-byte report + 3-byte ATT header)",
        timeout=20,
    )
    device.expect_exact("DEVICE_MTU_31 success=0 error=INVALID_ARGUMENT", timeout=20)
    device.expect_exact("DEVICE_MTU_32 success=1 error=NONE", timeout=20)
    # Restored and still NKRO: the rejected attempts must not have dropped the
    # keyboard configuration on the way through.
    device.expect_exact("DEVICE_MTU_RESTORED success=1 nkro=1", timeout=20)
