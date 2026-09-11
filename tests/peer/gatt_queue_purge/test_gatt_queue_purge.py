import re


def test_disconnect_defers_and_purges_queued_gatt_operations(dut, peers):
    """Two behaviours that only show up when a connection ends with GATT work
    outstanding, and that both fail silently if broken.

    `disconnect()` during an in-flight GATT operation must be DEFERRED, not
    refused: the operation finishes and the disconnect follows. Refusing it returns
    false, which reads to the application as "still connected" right after it asked
    to disconnect.

    Queued-but-unstarted operations must be PURGED with a failure completion each.
    Dropping them silently leaves the application waiting for callbacks that can
    never arrive, and leaves them clogging the single-slot queue ahead of a live
    connection.

    One sequence covers both: queue four reads (the first goes on the air, three
    wait), then ask to disconnect. The three waiting ones are purged straight away;
    the one on the air is left alone and completes, and its success arriving before
    the disconnect is what proves the deferral — it cannot happen if the connection
    had been torn down under the running worker.

    Purging at the disconnect request, rather than waiting for the connection to
    actually go, is what keeps the disconnect from being starved: update() pumps the
    GATT queue before it drains pending disconnects, so a queue left in place would
    have the next operation started on every pass.
    """
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("PERIPHERAL_READY advertising=1 chars=4", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect(re.compile(rb"CENTRAL_CONNECTED id=(\d+)"), timeout=20)
    peripheral.expect(re.compile(rb"PERIPHERAL_CONNECTED id=(\d+)"), timeout=20)
    dut.expect_exact("DISCOVERED success=1 characteristics=4", timeout=20)

    # All four are accepted: the queue holds 8 beside the one in flight.
    dut.write("q")
    dut.expect_exact("PURGE_SETUP queued=4 disconnect=1 error=NONE", timeout=10)

    # The three that never started are dropped at the moment the disconnect is
    # requested, each with its own failure completion naming the characteristic it
    # was for. error=1 is EspBleError::InvalidState.
    purged = []
    for _ in range(3):
        failure = dut.expect(re.compile(
            rb"READ tag=(\d) success=0 error=(\d+) value= detail=([^\r\n]*)"), timeout=20)
        assert failure.group(2) == b"1", "a purged operation fails with InvalidState"
        assert failure.group(3) == b"connection closed before the queued GATT operation started"
        purged.append(failure.group(1).decode())

    assert sorted(purged) == ["2", "3", "4"], \
        f"all three queued reads must be completed, got {purged}"

    # The one already on the air is NOT cancelled: it completes normally, and its
    # success arriving before the disconnect is the observable proof that the
    # disconnect was deferred rather than torn down under the running worker.
    first = dut.expect(re.compile(
        rb"READ tag=(\d) success=1 error=0 value=(v\d) detail="), timeout=20)
    assert first.group(1) == b"1", "the first queued read is the one that went on the air"
    assert first.group(2) == b"v1", "it must return the peer's value, not a failure"

    dut.expect(re.compile(rb"CENTRAL_DISCONNECTED id=(\d+)"), timeout=20)
    peripheral.expect(re.compile(rb"PERIPHERAL_DISCONNECTED id=(\d+)"), timeout=20)

    # If the event queue had overflowed, the checks above would have been reading an
    # incomplete picture.
    dut.write("c")
    dut.expect_exact("DROPPED events=0", timeout=10)

    # The stack is usable afterwards: nothing is stuck holding the single ATT slot.
    peripheral.write("?")
    peripheral.expect_exact("PERIPHERAL_READY advertising=1 chars=4", timeout=20)
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect(re.compile(rb"CENTRAL_CONNECTED id=(\d+)"), timeout=20)
    dut.expect_exact("DISCOVERED success=1 characteristics=4", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"CENTRAL_DISCONNECTED id=(\d+)"), timeout=20)
