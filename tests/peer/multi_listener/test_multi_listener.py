"""Multi-observer event delivery.

Every event slot carries one primary callback (the `on*()` setter) plus up to
four additional listeners (`add*Listener()`). Nothing verified that all of them
actually receive an event, that removing one leaves the rest alone, or that the
list refuses an add past capacity instead of dropping an existing observer.
Both owners of such a list are covered: EspBleGattServer (peripheral side) and
EspBle itself (central side).
"""

import time


def test_every_observer_receives_the_event_and_removal_is_selective(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=15)

    dut.write("z")
    dut.expect_exact("WRITE_STATE_RESET", timeout=10)
    peripheral.write("z")
    peripheral.expect_exact("WRITE_STATE_RESET", timeout=10)

    dut.write("c")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=15)
    dut.expect("CENTRAL_CONNECTED id=", timeout=20)
    peripheral.expect("PERIPHERAL_CONNECTED id=", timeout=10)
    dut.expect_exact("CENTRAL_READY success=1", timeout=15)

    # One write: the primary and both listeners see it, on both sides.
    dut.write("w")
    dut.expect_exact("WRITE_REQUESTED", timeout=10)
    dut.expect_exact("WRITE_DONE success=1", timeout=15)
    time.sleep(1)

    dut.write("s")
    dut.expect_exact("WRITE_STATE primary=1 first=1 second=1", timeout=10)
    peripheral.write("s")
    peripheral.expect_exact("WRITE_STATE value=hello primary=1 first=1 second=1", timeout=10)

    # Removing one listener must leave the primary and the other listener intact.
    dut.write("r")
    dut.expect_exact("LISTENER_REMOVED success=1", timeout=10)
    peripheral.write("r")
    peripheral.expect_exact("LISTENER_REMOVED success=1", timeout=10)

    dut.write("w")
    dut.expect_exact("WRITE_REQUESTED", timeout=10)
    dut.expect_exact("WRITE_DONE success=1", timeout=15)
    time.sleep(1)

    dut.write("s")
    dut.expect_exact("WRITE_STATE primary=2 first=2 second=1", timeout=10)
    peripheral.write("s")
    peripheral.expect_exact("WRITE_STATE value=hello primary=2 first=1 second=2", timeout=10)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect("CENTRAL_DISCONNECTED id=", timeout=15)


def test_connection_events_reach_primary_then_listeners(dut, peers):
    """Connection events use the same primary + listeners model as the GATT
    events, so an integration layer can follow connections without taking the
    application's `on*()` slot.

    Two properties that counts alone would not show: the primary runs before the
    listeners and the listeners run in registration order (checked through the
    recorded order string), and `removeConnectionListener()` drops exactly one
    observer while the primary and the remaining listener keep firing.
    """
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=15)

    dut.write("j")
    dut.expect_exact("CONN_STATE_RESET", timeout=10)

    dut.write("c")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=15)
    dut.expect("CENTRAL_CONNECTED id=", timeout=20)
    peripheral.expect("PERIPHERAL_CONNECTED id=", timeout=10)
    time.sleep(1)

    dut.write("k")
    dut.expect_exact(
        "CONN_STATE primary=1 first=1 second=1 disconnected=0 order=P12", timeout=10
    )

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect("CENTRAL_DISCONNECTED id=", timeout=15)
    time.sleep(1)

    dut.write("k")
    dut.expect_exact(
        "CONN_STATE primary=1 first=1 second=1 disconnected=1 order=P12", timeout=10
    )

    # Removing one connected listener must leave the primary and the other one.
    dut.write("x")
    dut.expect_exact("CONN_LISTENER_REMOVED success=1", timeout=10)

    dut.write("c")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=15)
    dut.expect("CENTRAL_CONNECTED id=", timeout=20)
    time.sleep(1)

    dut.write("k")
    dut.expect_exact(
        "CONN_STATE primary=2 first=2 second=1 disconnected=1 order=P12P1", timeout=10
    )

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect("CENTRAL_DISCONNECTED id=", timeout=15)


def test_listener_list_refuses_unknown_removal_and_overflow(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=15)

    # An id that was never issued must be reported as not removed.
    peripheral.write("u")
    peripheral.expect_exact("LISTENER_REMOVE_UNKNOWN success=0", timeout=10)

    # The list holds four listeners besides the primary. Filling it must stop at
    # that total: the add past capacity is refused rather than evicting an
    # existing observer. The sketch reports the absolute total so this does not
    # depend on how many the earlier test left registered.
    peripheral.write("F")
    peripheral.expect_exact("total=4", timeout=10)
