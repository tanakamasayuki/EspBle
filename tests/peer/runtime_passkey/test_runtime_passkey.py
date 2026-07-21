import re


def test_runtime_passkey_entry(dut, peers):
    """Interactive runtime Passkey Entry. The peripheral (DisplayOnly, MITM, no
    static passkey) generates a random passkey per pairing and surfaces it via
    onPasskeyDisplayed. The central (KeyboardOnly, MITM, no static passkey)
    connects, its backend passkey request blocks, and the test relays the
    displayed passkey to the central via providePasskey(); pairing then completes
    authenticated and bonded on both sides."""
    peripheral = peers["device"]

    dut.write("x")
    peripheral.write("x")
    dut.expect_exact("CENTRAL_BONDS_CLEARED success=1 count=0", timeout=10)
    peripheral.expect_exact("PERIPHERAL_BONDS_CLEARED success=1 count=0", timeout=10)

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=20)
    dut.expect_exact("CENTRAL_CONNECTED id=1", timeout=20)

    # The peripheral generates and displays a random passkey during pairing.
    displayed = peripheral.expect(
        re.compile(rb"PASSKEY_DISPLAYED id=1 passkey=(\d{6}) context=loop"), timeout=20)
    passkey = displayed.group(1).decode()

    # Relay it to the central, which is blocking in its passkey request.
    dut.write("k" + passkey + "\n")
    dut.expect_exact("PASSKEY_PROVIDED accepted=1 passkey=" + passkey, timeout=10)

    dut.expect_exact(
        "CENTRAL_SECURITY success=1 encrypted=1 authenticated=1 bonded=1 key=16 context=loop",
        timeout=20)
    peripheral.expect_exact(
        "PERIPHERAL_SECURITY success=1 encrypted=1 authenticated=1 bonded=1 key=16 context=loop",
        timeout=20)

    dut.write("d")
    dut.expect_exact("DISCONNECT_REQUESTED", timeout=10)
    dut.expect_exact("CENTRAL_DISCONNECTED id=1 context=loop", timeout=20)
    peripheral.expect_exact("PERIPHERAL_DISCONNECTED id=1 context=loop", timeout=20)

    dut.write("x")
    peripheral.write("x")
    dut.expect_exact("CENTRAL_BONDS_CLEARED success=1 count=0", timeout=10)
    peripheral.expect_exact("PERIPHERAL_BONDS_CLEARED success=1 count=0", timeout=10)
