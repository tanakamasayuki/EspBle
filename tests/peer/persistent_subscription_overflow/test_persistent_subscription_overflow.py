import re


def test_persistent_subscription_registry_overflow_is_counted(dut, peers):
    """The persistent subscription registry holds 16 records, keyed by peer
    address + service + characteristic. Overflow must be counted rather than
    dropped in silence, because a lost record means that subscription will not be
    restored on the next connection and nothing else would tell the application.

    One address cannot fill it: the central's active subscription table is also 16
    and fills first, and a subscribe rejected there never reaches the CCCD write,
    so no record is made. Reconnecting to the same address does not help either —
    the records are restored automatically and take the active table again. So the
    peer re-inits with a random static address between the two batches, which the
    central sees as a different peer: nothing is restored, and the records keep
    accumulating past 16.
    """
    peripheral = peers["device"]

    peripheral.write("?")
    ready = peripheral.expect(re.compile(
        rb"PERIPHERAL_READY advertising=(\d+) chars=(\d+) address=([0-9a-fA-F:]+)"),
        timeout=20)
    assert ready.group(1) == b"1", "peripheral must be advertising"
    assert int(ready.group(2)) == 12, "peripheral must expose 12 notifiable characteristics"
    public_address = ready.group(3).decode()

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect(re.compile(rb"CENTRAL_CONNECTED id=(\d+)"), timeout=20)
    peripheral.expect(re.compile(rb"PERIPHERAL_CONNECTED id=(\d+)"), timeout=20)
    discovered = dut.expect(re.compile(
        rb"DISCOVERED success=(\d+) characteristics=(\d+)"), timeout=20)
    assert discovered.group(1) == b"1", "discovery must succeed"
    assert int(discovered.group(2)) == 12

    # Batch 1: 12 records under the public address. Nothing is dropped yet.
    dut.write("1")
    dut.expect_exact("BATCH_STARTED count=12", timeout=10)
    first = dut.expect(re.compile(
        rb"BATCH_DONE subscribed=(\d+) failed=(\d+) dropped=(\d+)"), timeout=60)
    assert int(first.group(1)) == 12, "all 12 subscribes must succeed"
    assert int(first.group(2)) == 0
    assert int(first.group(3)) == 0, "the registry has room for 12"

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"CENTRAL_DISCONNECTED id=(\d+)"), timeout=20)
    peripheral.expect(re.compile(rb"PERIPHERAL_DISCONNECTED id=(\d+)"), timeout=20)

    # The records survive the disconnect: that is what "persistent" means.
    dut.write("c")
    dut.expect_exact("DROPPED count=0", timeout=10)

    # The peer comes back as a different address, so the central restores nothing.
    peripheral.write("R")
    readdressed = peripheral.expect(re.compile(
        rb"PERIPHERAL_READDRESSED success=(\d+) address=([0-9a-fA-F:]+)"), timeout=30)
    assert readdressed.group(1) == b"1", "re-init must succeed"
    random_static_address = readdressed.group(2).decode()
    assert random_static_address.lower() != public_address.lower(), \
        "the random static address must differ from the public one"

    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect(re.compile(rb"CENTRAL_CONNECTED id=(\d+)"), timeout=20)
    dut.expect(re.compile(rb"DISCOVERED success=1 characteristics=12"), timeout=20)

    dut.write("a")
    peer_address = dut.expect(re.compile(rb"PEER_ADDRESS ([0-9a-fA-F:]+)"), timeout=10)
    assert peer_address.group(1).decode().lower() == random_static_address.lower(), \
        "the central must see the new address, otherwise the records would collide"

    # Batch 2: records 13-16 fit, the 17th does not. The subscribe itself still
    # succeeds — only the record is lost, which is exactly why it must be counted.
    dut.write("2")
    dut.expect_exact("BATCH_STARTED count=5", timeout=10)
    second = dut.expect(re.compile(
        rb"BATCH_DONE subscribed=(\d+) failed=(\d+) dropped=(\d+)"), timeout=60)
    assert int(second.group(1)) == 5, "all 5 subscribes must still succeed on the air"
    assert int(second.group(2)) == 0
    assert int(second.group(3)) == 1, \
        "the 17th record must be counted as dropped (16 + 1)"

    dut.write("c")
    dut.expect_exact("DROPPED count=1", timeout=10)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect(re.compile(rb"CENTRAL_DISCONNECTED id=(\d+)"), timeout=20)
