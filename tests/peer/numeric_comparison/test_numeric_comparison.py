import re


def test_numeric_comparison(dut, peers):
    """LE Secure Connections Numeric Comparison. Both devices (DisplayYesNo,
    MITM) display the same 6-digit value via onNumericComparison; the test checks
    the two values match, confirms on both sides with confirmNumericComparison(),
    and pairing then completes authenticated and bonded."""
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

    # Both sides display the same comparison value.
    central_value = dut.expect(
        re.compile(rb"CENTRAL_NUMCMP id=1 value=(\d{6}) context=loop"), timeout=20).group(1)
    peripheral_value = peripheral.expect(
        re.compile(rb"PERIPHERAL_NUMCMP id=1 value=(\d{6}) context=loop"), timeout=20).group(1)
    assert central_value == peripheral_value, (
        f"comparison values differ (central={central_value}, peripheral={peripheral_value})")

    # Confirm the match on both sides.
    dut.write("y")
    dut.expect_exact("CENTRAL_CONFIRM accepted=1", timeout=10)
    peripheral.write("y")
    peripheral.expect_exact("PERIPHERAL_CONFIRM accepted=1", timeout=10)

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
