import re


def test_fitness_machine_service(dut, peers):
    """The Fitness Machine server notifies Indoor Bike Data with flags-driven
    fields; the client reads Fitness Machine Feature, subscribes, and decodes
    instantaneous speed, cadence, and power."""
    device = peers["device"]

    device.write("?")
    device.expect_exact("ADVERTISING 1", timeout=20)

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("FEATURE_READ_REQUESTED", timeout=20)

    match = dut.expect(
        re.compile(rb"FEATURE_READ valid=(\d+) features=(\d+) context=(\w+)"), timeout=20
    )
    assert match.group(1) == b"1", "Fitness Machine Feature read failed (expected 8 bytes)"
    assert int(match.group(2)) == 6, "expected Fitness Machine Features 0x00000006"
    assert match.group(3) == b"loop"

    dut.expect_exact("FTMS_SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("FTMS_SUBSCRIBED success=1", timeout=20)
    device.expect(re.compile(rb"FTMS_SUBSCRIPTION notifications=1"), timeout=20)

    device.write("h")
    device.expect_exact("FTMS_UPDATED stored=1 notified=1", timeout=10)
    measurement = dut.expect(
        re.compile(
            rb"FTMS_BIKE valid=(\d+) flags=([0-9a-f]{4}) speed=(\d+) cadence=(\d+) power=(-?\d+) context=(\w+)"
        ),
        timeout=20,
    )
    assert measurement.group(1) == b"1", "Indoor Bike Data too short"
    assert measurement.group(2) == b"0044", "expected flags 0x0044 (speed+cadence+power)"
    assert int(measurement.group(3)) == 3000, "instantaneous speed (0.01 km/h) did not decode to 3000"
    assert int(measurement.group(4)) == 90, "instantaneous cadence did not decode to 90 rpm"
    assert int(measurement.group(5)) == 250, "instantaneous power did not decode to 250 W"
    assert measurement.group(6) == b"loop", "must be delivered from update()/loop"

    device.expect(re.compile(rb"FTMS_SENT success=1"), timeout=20)

    # Control Point: subscribe the Control Point (indications) and Fitness
    # Machine Status (notifications).
    dut.write("c")
    dut.expect_exact("CONTROL_SUBSCRIBE_REQUESTED cp=1 status=1", timeout=10)
    dut.expect_exact("CP_SUBSCRIBED success=1", timeout=20)
    dut.expect_exact("STATUS_SUBSCRIBED success=1", timeout=20)

    # Set Target Power (op 0x05, 250 W) -> Control Point response indication.
    # (A single Control Point indication is exercised: BLE allows only one
    # indication in flight per connection, so back-to-back Control Point ops
    # would need each response confirmed before the next.)
    dut.write("p")
    dut.expect_exact("SET_TARGET_POWER_REQUESTED", timeout=10)
    device.expect_exact("CP_WRITE op=05 set_target_power=250 context=loop", timeout=20)
    dut.expect_exact(
        "CP_RESPONSE valid=1 op=05 result=1 indication=1 context=loop", timeout=20
    )
    # Wait for the Control Point indication send to complete before the next
    # send (BLE sends are single-in-flight on the server).
    device.expect_exact("FTMS_SENT success=1 length=3 context=loop", timeout=20)

    # The server reports the change via a Fitness Machine Status "Target Power
    # Changed" (0x08) notification carrying the value set above.
    device.write("g")
    device.expect_exact("STATUS_SENT notified=1 power=250", timeout=10)
    dut.expect_exact("FTMS_STATUS type=08 power=250 context=loop", timeout=20)

    dut.write("u")
    dut.expect_exact("FTMS_UNSUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact("FTMS_UNSUBSCRIBED success=1", timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"DISCONNECTED id=(\d+)"), timeout=20)
