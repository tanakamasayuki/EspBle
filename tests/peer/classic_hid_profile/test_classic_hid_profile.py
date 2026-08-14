import re


def test_classic_hid_profiles_match_the_ble_api(dut, peers):
    """Classic HID through the profile API, decoded by the Classic HID Host.

    Both sides use the calls and the event shapes the BLE side uses. What makes
    this a real check rather than a loopback is that the Host decodes from the
    Report Descriptor it received over SDP: if the composed descriptor and the
    packed reports disagree, the usages arrive wrong or not at all.
    """
    peer = peers["device"]
    peer.expect_exact("HOST_READY", timeout=30)
    ready = dut.expect(
        re.compile(rb"DEVICE_READY address=([0-9a-f:]+) keyboard=1 mouse=1"),
        timeout=30,
    )
    address = ready.group(1).decode()

    peer.write("c" + address + "\n")
    peer.expect_exact("HOST_CONNECT requested=1", timeout=20)
    peer.expect(re.compile(rb"HOST_CONNECTED peer=" + address.encode()), timeout=40)
    dut.expect(re.compile(rb"DEVICE_CONNECTED peer=[0-9a-f:]+"), timeout=30)

    # The descriptor has to reach the Host before any report can be decoded.
    peer.write("?\n")
    peer.expect(re.compile(rb"HOST_STATE connected=1 map=1 invalid=0"), timeout=20)

    # A key press: the state snapshot comes first, then the per-usage event,
    # the same order the BLE host uses. usage 0x04 is "a" on en-US.
    dut.write("k")
    dut.expect_exact("DEVICE_KEY sent=1", timeout=10)
    peer.expect_exact("HOST_STATE modifiers=0 a=1", timeout=20)
    peer.expect(
        re.compile(rb"HOST_KEY usage=4 ascii=97 pressed=1 released=0 modifiers=0 raw=(\d+)"),
        timeout=20,
    )

    dut.write("r")
    dut.expect_exact("DEVICE_RELEASE sent=1", timeout=10)
    peer.expect_exact("HOST_STATE modifiers=0 a=0", timeout=20)
    peer.expect(
        re.compile(rb"HOST_KEY usage=4 ascii=97 pressed=0 released=1 modifiers=0 raw=\d+"),
        timeout=20,
    )

    # The layout lives on each side independently: the device picks the usage
    # for a character, the host names the usage for its own layout. Sending '"'
    # on ja-JP produces usage 0x1f, which en-US would have reached with '2'.
    dut.write("l")
    dut.expect_exact("DEVICE_LAYOUT ja=1", timeout=10)
    dut.write("q")
    dut.expect_exact("DEVICE_QUOTE sent=1", timeout=10)
    peer.expect(
        re.compile(rb"HOST_KEY usage=31 ascii=64 pressed=1 released=0 modifiers=2 raw=\d+"),
        timeout=20,
    )
    dut.write("r")
    dut.expect_exact("DEVICE_RELEASE sent=1", timeout=10)

    # Now teach the host the same layout and repeat: the usage is unchanged and
    # only the character the host derives from it moves.
    peer.write("j\n")
    peer.expect_exact("HOST_LAYOUT ja-JP", timeout=10)
    dut.write("q")
    dut.expect_exact("DEVICE_QUOTE sent=1", timeout=10)
    peer.expect(
        re.compile(rb"HOST_KEY usage=31 ascii=34 pressed=1 released=0 modifiers=2 raw=\d+"),
        timeout=20,
    )
    dut.write("r")
    dut.expect_exact("DEVICE_RELEASE sent=1", timeout=10)

    # Mouse movement decodes through the descriptor's field positions, so a
    # signed value must survive as a signed value.
    dut.write("m")
    dut.expect_exact("DEVICE_MOUSE sent=1 buttons=0", timeout=10)
    peer.expect(
        re.compile(rb"HOST_MOUSE x=5 y=-3 wheel=0 buttons=0 moved=1 changed=0"),
        timeout=20,
    )

    # A held button is state, not an edge: it stays set until it is released.
    dut.write("c")
    dut.expect_exact("DEVICE_CLICK sent=1 buttons=1", timeout=10)
    peer.expect(
        re.compile(rb"HOST_MOUSE x=0 y=0 wheel=0 buttons=1 moved=0 changed=1"),
        timeout=20,
    )
    dut.write("v")
    dut.expect_exact("DEVICE_RELEASE_BUTTONS sent=1 buttons=0", timeout=10)
    peer.expect(
        re.compile(rb"HOST_MOUSE x=0 y=0 wheel=0 buttons=0 moved=0 changed=1"),
        timeout=20,
    )

    # Consumer Control shares the same HID Device, so its report must reach the
    # host as its own report ID rather than being decoded as a keyboard.
    dut.write("u")
    dut.expect_exact("DEVICE_CONSUMER sent=1", timeout=10)
    peer.expect(re.compile(rb"HOST_RAW id=4 len=2"), timeout=20)

    # Nothing above should have looked malformed to the host.
    peer.write("?\n")
    peer.expect(re.compile(rb"HOST_STATE connected=1 map=1 invalid=0"), timeout=20)
    dut.write("?")
    dut.expect(re.compile(rb"DEVICE_STATE connected=1 ready=1 heap=\d+"), timeout=10)
