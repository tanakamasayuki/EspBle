import re

HANDLES = re.compile(rb"HANDLES first=(\d+) duplicate=(\d+) other=(\d+)")


def test_duplicate_uuid_registration(dut, peers):
    """Two services may share a UUID; two characteristics in one service may not.

    The bundled backend reuses an existing characteristic when a second one is
    added with the same UUID in the same service, so EspBle rejects that instead
    of returning a handle whose sends would go nowhere.
    """
    peripheral = peers["device"]

    peripheral.write("h")
    match = peripheral.expect(HANDLES, timeout=15)
    first, duplicate, other = (int(match.group(i)) for i in (1, 2, 3))
    assert first != 0, "the first characteristic should register"
    assert duplicate == 0, "a duplicate UUID in one service must be refused"
    assert other != 0, "the same UUID in a second service should register"
    assert first != other

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    dut.write("c")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect("CENTRAL_CONNECTED id=", timeout=20)

    # The peer registered the service twice, but the bundled backend keys remote
    # services by UUID on the client side, so only the first is enumerable here.
    dut.expect_exact("DISCOVERED services=1 characteristics=1", timeout=20)
    dut.expect_exact("READ handle=", timeout=15)
    dut.expect_exact("READ_DONE", timeout=10)
