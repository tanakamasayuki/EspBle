import re

import pexpect


def probe(target, command, pattern, attempts=12, timeout=5):
    """Ask until answered, instead of waiting for a banner printed once.

    A sketch prints its ready line at boot, which is gone if the monitor attaches
    after the reset. A command probe cannot be missed that way.
    """
    for _ in range(attempts):
        target.write(command)
        try:
            return target.expect(pattern, timeout=timeout)
        except pexpect.TIMEOUT:
            continue
    raise AssertionError(f"no answer to {command!r} matching {pattern!r}")


def test_classic_radio_settings_apply_to_the_radio(dut):
    """Transmit power, page timeout and minimum encryption key size.

    Accepting a call proves nothing here, so the page timeout is checked by
    timing a connection attempt at an address nothing answers on: the attempt
    ends when paging gives up, so shortening the timeout has to shorten the
    attempt. The transmit power is read back because the radio has discrete
    levels and a value between two of them is rounded rather than refused.
    """
    probe(dut, "?\n", re.compile(rb"STATE attempt=0 heap=\d+"))

    # The library asks the controller for its page timeout at startup, so the
    # default is known without a sketch setting one. 0x2000 slots = 5120 ms.
    dut.write("p\n")
    dut.expect_exact("PAGE_TIMEOUT ms=5120", timeout=20)

    # A range, both ends pinned, and a value between two supported levels.
    dut.write("w\n")
    dut.expect_exact("TX_SET range=1", timeout=10)
    dut.expect_exact("TX_POWER range read=1 min=-12 max=9 single=9", timeout=10)
    dut.expect_exact("TX_SET single=1", timeout=10)
    dut.expect_exact("TX_POWER single read=1 min=0 max=0 single=0", timeout=10)
    dut.expect_exact("TX_SET rounded=1", timeout=10)
    dut.expect_exact("TX_POWER rounded read=1 min=-6 max=-6 single=-6", timeout=10)

    # A range whose minimum exceeds its maximum is a mistake, not a rounding
    # question, so it is refused before the radio sees it.
    dut.write("x\n")
    dut.expect_exact("TX_REJECT inverted=0 error=InvalidArgument", timeout=10)
    dut.write("r\n")
    dut.expect_exact("TX_POWER current read=1 min=-6 max=-6 single=-6", timeout=10)

    # Out of range in both directions: 5 ms is below the controller's 0x0016
    # slots and 50000 ms above 0xffff.
    dut.write("s5\n")
    dut.expect_exact("PAGE_SET requested=5 accepted=0 error=InvalidArgument", timeout=10)
    dut.write("s50000\n")
    dut.expect_exact(
        "PAGE_SET requested=50000 accepted=0 error=InvalidArgument", timeout=10
    )
    dut.write("p\n")
    dut.expect_exact("PAGE_TIMEOUT ms=5120", timeout=10)

    # A short timeout, confirmed by the backend and then by how long an attempt
    # at an absent peer runs for.
    dut.write("s1000\n")
    dut.expect_exact("PAGE_SET requested=1000 accepted=1 error=None", timeout=10)
    dut.write("p\n")
    dut.expect_exact("PAGE_TIMEOUT ms=1000", timeout=20)

    dut.write("c\n")
    dut.expect_exact("ATTEMPT_STARTED requested=1", timeout=10)
    short = dut.expect(
        re.compile(rb"ATTEMPT_FAILED elapsed=(\d+) peer=02:00:00:00:00:01"),
        timeout=30,
    )
    short_elapsed = int(short.group(1))
    assert short_elapsed < 3000, short_elapsed

    # The same attempt with the default timeout has to take visibly longer,
    # which is what makes the setting a setting and not a stored number.
    dut.write("s5120\n")
    dut.expect_exact("PAGE_SET requested=5120 accepted=1 error=None", timeout=10)
    dut.write("p\n")
    dut.expect_exact("PAGE_TIMEOUT ms=5120", timeout=20)
    dut.write("c\n")
    dut.expect_exact("ATTEMPT_STARTED requested=1", timeout=10)
    default = dut.expect(
        re.compile(rb"ATTEMPT_FAILED elapsed=(\d+) peer=02:00:00:00:00:01"),
        timeout=40,
    )
    default_elapsed = int(default.group(1))
    assert default_elapsed > short_elapsed + 1000, (short_elapsed, default_elapsed)

    # Key size: the maximum is accepted, and both ends of the invalid range are
    # refused locally rather than being sent as a nonsense policy.
    dut.write("k16\n")
    dut.expect_exact("KEY_SIZE requested=16 accepted=1 error=None", timeout=10)
    dut.write("k6\n")
    dut.expect_exact("KEY_SIZE requested=6 accepted=0 error=InvalidArgument", timeout=10)
    dut.write("k17\n")
    dut.expect_exact(
        "KEY_SIZE requested=17 accepted=0 error=InvalidArgument", timeout=10
    )

    dut.write("?\n")
    dut.expect(re.compile(rb"STATE attempt=0 heap=\d+"), timeout=10)
