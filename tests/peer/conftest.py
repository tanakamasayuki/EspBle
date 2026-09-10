"""The reserved stop command for the peer test sketches.

A test that fails halfway — an `expect` timeout with a link still up, a scan
still running — never reaches the commands at the end of its own body, and a run
that ends leaves the boards doing whatever the last test left them doing. Two
boards advertising into the next fixture's scan is the failure that costs the
most time to diagnose. Fixture teardown runs whatever the outcome, including
Ctrl-C, which is why the cleanup lives here rather than at the end of each test.

There is nothing to restore. Every module holds one test and every module begins
with an upload, which resets the board, so no state is carried from one test to
the next; the command exists so that a board stops using the radio when its test
ends rather than at the next upload. One command goes to the primary DUT and
then to every connected peer, and is answered by
`tests/sketch_support/EspBleTestLifecycle.h`:

    0x04 (EOT)  STOP  nothing connected, advertising or scanning

The reply means the sketch has reached that state, not that the byte arrived.
Nothing here can fail a test: a board that does not answer costs one timeout and
a note, which is what a hung or crashed sketch looks like.

This does not replace what a test does at its start. A test still brings the
state it needs into place with its own commands (see `lifecycle_stress`), both
because it must not depend on the previous test's cleanup having worked and
because it must run alone under `-k`.
"""

import os

import pytest

STOP = b"\x04\n"
STOP_REPLY = "STOPPED"
REPLY_TIMEOUT = 2


def _mark(device, text):
    """Put a line into the board's log so the exchange is visible next to its output.

    The line goes into the message queue the log is written from, not out of the
    serial port: sending it to the board would hand the sketch a stray command.
    """
    queue = getattr(device, "_q", None)
    if queue is None:
        return
    try:
        queue.put(f"[espble] {text}\n".encode())
    except Exception:  # noqa: BLE001 - a missing marker is not worth a failure
        pass


def _stop(device, name):
    """Best effort: a board that cannot answer must not change the result."""
    _mark(device, f"STOP -> {name}")
    try:
        device.write(STOP)
        device.expect_exact(STOP_REPLY, timeout=REPLY_TIMEOUT)
    except Exception as error:  # noqa: BLE001 - cleanup never fails a test
        print(f"\ncleanup: {name} did not answer STOP: {type(error).__name__}")


@pytest.fixture(autouse=True)
def stop_radio(dut, peers):
    """Take every board off the air once its test is over.

    Requesting `dut` puts this fixture after the port is open and before it
    closes, so the command goes out over a live connection. The primary is sent
    first and the peers in reverse name order, the same order in which they are
    closed.
    """
    yield

    _stop(dut, "primary")
    for name in sorted(peers, reverse=True):
        _stop(peers[name], f"peer {name}")


@pytest.fixture
def run_checks():
    """Run a merged test's cases, in order or back to front.

    Merging several tests into one moves the ordering out of pytest and into
    the test, so the check that the cases do not depend on each other has to
    move with it: a list can be reversed, a sequence of direct calls cannot.
    Set `ESPBLE_REVERSE_CHECKS=1` to run them backwards.

    Failures are not caught. The traceback names the case that failed and
    prints the line and the value it was waiting for, which is more than any
    summary this fixture could write; and after a failure on hardware the board
    is in no state for the remaining cases to mean anything.
    """

    def run(checks, *args):
        order = list(checks)
        if os.environ.get("ESPBLE_REVERSE_CHECKS") == "1":
            order.reverse()
        for check in order:
            check(*args)

    return run
