# The MTU case ends and re-begins the device's BLE stack, which drops any
# link, so it cannot share a board with the cases that need one. Its own
# module gets its own upload and needs no link at all.


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
