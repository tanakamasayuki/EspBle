def test_hid_convenience_input_apis(dut, peers):
    device = peers["device"]

    dut.write("x")
    device.write("x")
    dut.expect_exact("HOST_BONDS_CLEARED success=1 count=0", timeout=10)
    device.expect_exact("DEVICE_BONDS_CLEARED success=1 count=0", timeout=10)

    device.write("?")
    device.expect_exact("DEVICE_ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("HOST_SCAN_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_CONNECT_STARTED success=1", timeout=20)
    dut.expect_exact("HOST_CONNECTED id=1", timeout=20)
    device.expect_exact("DEVICE_CONNECTED id=1", timeout=20)
    dut.expect_exact("HOST_SECURITY encrypted=1 bonded=1", timeout=20)
    dut.expect_exact("HOST_DISCOVERY_STARTED success=1", timeout=10)
    dut.expect_exact(
        "HOST_DISCOVERED success=1 report=1 country=13 battery=55 context=loop detail=",
        timeout=20,
    )

    # pressKey() maps a character to a usage in the device-side layout. 'a' is
    # usage 0x04 with no modifier; 'A' is the same usage with Shift, which the
    # caller never names.
    device.write("a")
    device.expect_exact("DEVICE_PRESS_KEY success=1", timeout=10)
    dut.expect_exact("HOST_KEY usage=4 ascii=97 pressed=1 released=0 modifiers=0", timeout=20)
    device.write("r")
    device.expect_exact("DEVICE_RELEASE_ALL success=1", timeout=10)
    dut.expect_exact("HOST_KEY usage=4 ascii=97 pressed=0 released=1 modifiers=0", timeout=20)

    device.write("A")
    device.expect_exact("DEVICE_PRESS_KEY_SHIFTED success=1", timeout=10)
    dut.expect_exact("HOST_KEY usage=4 ascii=65 pressed=1 released=0 modifiers=2", timeout=20)
    device.write("r")
    device.expect_exact("DEVICE_RELEASE_ALL success=1", timeout=10)

    # tapKey() is press + hold + releaseAll, so both edges arrive from one call.
    device.write("t")
    device.expect_exact("DEVICE_TAP_KEY success=1", timeout=10)
    dut.expect_exact("HOST_KEY usage=5 ascii=98 pressed=1 released=0 modifiers=0", timeout=20)
    dut.expect_exact("HOST_KEY usage=5 ascii=98 pressed=0 released=1 modifiers=0", timeout=20)

    # write() taps every character in order: h (0x0b) then i (0x0c).
    device.write("w")
    device.expect_exact("DEVICE_WRITE success=1", timeout=15)
    dut.expect_exact("HOST_KEY usage=11 ascii=104 pressed=1 released=0 modifiers=0", timeout=20)
    dut.expect_exact("HOST_KEY usage=11 ascii=104 pressed=0 released=1 modifiers=0", timeout=20)
    dut.expect_exact("HOST_KEY usage=12 ascii=105 pressed=1 released=0 modifiers=0", timeout=20)
    dut.expect_exact("HOST_KEY usage=12 ascii=105 pressed=0 released=1 modifiers=0", timeout=20)

    # A character no key produces is refused locally; nothing goes on the air.
    device.write("Z")
    device.expect_exact(
        "DEVICE_PRESS_KEY_UNMAPPED success=0 error=INVALID_ARGUMENT", timeout=10
    )

    # Usage-level APIs reach keys pressKey() cannot name. Escape is 0x29 and
    # decodes to ASCII 0x1b; F1 (0x3a) has no character at all.
    device.write("u")
    device.expect_exact("DEVICE_TAP_USAGE success=1", timeout=10)
    dut.expect_exact("HOST_KEY usage=41 ascii=27 pressed=1 released=0 modifiers=0", timeout=20)
    dut.expect_exact("HOST_KEY usage=41 ascii=27 pressed=0 released=1 modifiers=0", timeout=20)

    device.write("U")
    device.expect_exact("DEVICE_PRESS_USAGE success=1", timeout=10)
    dut.expect_exact("HOST_KEY usage=58 ascii=0 pressed=1 released=0 modifiers=1", timeout=20)
    device.write("q")
    device.expect_exact("DEVICE_RELEASE_USAGE success=1", timeout=10)
    dut.expect_exact("HOST_KEY usage=58 ascii=0 pressed=0 released=1 modifiers=0", timeout=20)

    # setLayout() changes which key pressKey() picks. '"' is Shift + ' (0x34) on
    # en-US and Shift + 2 (0x1f) on ja-JP. The host keeps its own en-US layout,
    # so only the usage differs — which is exactly what the device chose.
    device.write("E")
    device.expect_exact("DEVICE_LAYOUT lcid=1033", timeout=10)
    device.write("Q")
    device.expect_exact("DEVICE_PRESS_QUOTE success=1", timeout=10)
    dut.expect_exact("HOST_KEY usage=52 ascii=34 pressed=1 released=0 modifiers=2", timeout=20)
    device.write("r")
    device.expect_exact("DEVICE_RELEASE_ALL success=1", timeout=10)

    device.write("J")
    device.expect_exact("DEVICE_LAYOUT lcid=1041", timeout=10)
    device.write("Q")
    device.expect_exact("DEVICE_PRESS_QUOTE success=1", timeout=10)
    dut.expect_exact("HOST_KEY usage=31 ascii=64 pressed=1 released=0 modifiers=2", timeout=20)
    device.write("r")
    device.expect_exact("DEVICE_RELEASE_ALL success=1", timeout=10)
    device.write("E")
    device.expect_exact("DEVICE_LAYOUT lcid=1033", timeout=10)

    # wheel() moves nothing but the wheel and keeps the current buttons. The
    # host's moved flag covers the wheel as well as x/y, so it is set here.
    device.write("o")
    device.expect_exact("DEVICE_WHEEL success=1", timeout=10)
    dut.expect_exact("HOST_MOUSE x=0 y=0 wheel=3 buttons=0 moved=1 changed=0", timeout=20)

    # click() is press + hold + release, so the host sees both button edges.
    device.write("c")
    device.expect_exact("DEVICE_CLICK success=1", timeout=10)
    dut.expect_exact("HOST_MOUSE x=0 y=0 wheel=0 buttons=1 moved=0 changed=1", timeout=20)
    dut.expect_exact("HOST_MOUSE x=0 y=0 wheel=0 buttons=0 moved=0 changed=1", timeout=20)

    # press() accumulates buttons rather than replacing them.
    device.write("P")
    device.expect_exact("DEVICE_PRESS_BUTTONS success=1 buttons=3", timeout=10)
    dut.expect_exact("HOST_MOUSE x=0 y=0 wheel=0 buttons=1 moved=0 changed=1", timeout=20)
    dut.expect_exact("HOST_MOUSE x=0 y=0 wheel=0 buttons=3 moved=0 changed=1", timeout=20)
    device.write("p")
    device.expect_exact("DEVICE_MOUSE_RELEASE_ALL success=1 buttons=0", timeout=10)
    dut.expect_exact("HOST_MOUSE x=0 y=0 wheel=0 buttons=0 moved=0 changed=1", timeout=20)

    # sendUsage() records the usage it sent; click() ends back at 0.
    device.write("k")
    device.expect_exact("DEVICE_CONSUMER_USAGE success=1 usage=233", timeout=10)
    dut.expect_exact("HOST_CONSUMER usage=233 pressed=1 released=0", timeout=20)
    device.write("K")
    device.expect_exact("DEVICE_CONSUMER_CLICK success=1 usage=0", timeout=10)
    dut.expect_exact("HOST_CONSUMER usage=205 pressed=1 released=0", timeout=20)
    dut.expect_exact("HOST_CONSUMER usage=0 pressed=0 released=1", timeout=20)

    device.write("y")
    device.expect_exact("DEVICE_SYSTEM_USAGE success=1 usage=3", timeout=10)
    dut.expect_exact("HOST_SYSTEM usage=3 pressed=1 released=0", timeout=20)
    device.write("Y")
    device.expect_exact("DEVICE_SYSTEM_CLICK success=1 usage=0", timeout=10)
    dut.expect_exact("HOST_SYSTEM usage=2 pressed=1 released=0", timeout=20)
    dut.expect_exact("HOST_SYSTEM usage=0 pressed=0 released=1", timeout=20)

    # 6 axes + hat + 32 buttons = 39 parsed fields.
    device.write("g")
    device.expect_exact("DEVICE_GAMEPAD success=1", timeout=10)
    dut.expect_exact("HOST_GAMEPAD fields=39 changed=1 x=40 hat=7", timeout=20)
    device.write("G")
    device.expect_exact("DEVICE_GAMEPAD_RELEASE_ALL success=1", timeout=10)
    dut.expect_exact("HOST_GAMEPAD fields=39 changed=1 x=0 hat=0", timeout=20)

    dut.write("d")
    dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
    dut.expect("HOST_DISCONNECTED id=", timeout=20)
    device.expect("DEVICE_DISCONNECTED id=", timeout=20)

    dut.write("x")
    device.write("x")
    dut.expect_exact("HOST_BONDS_CLEARED success=1 count=0", timeout=10)
    device.expect_exact("DEVICE_BONDS_CLEARED success=1 count=0", timeout=10)


def test_hid_convenience_host_listeners(dut, peers):
    device = peers["device"]

    dut.write("x")
    device.write("x")
    dut.expect_exact("HOST_BONDS_CLEARED success=1 count=0", timeout=10)
    device.expect_exact("DEVICE_BONDS_CLEARED success=1 count=0", timeout=10)

    device.write("?")
    device.expect_exact("DEVICE_ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("HOST_SCAN_STARTED success=1", timeout=10)
    # The connection id depends on whether the boards were reset between tests,
    # so match the prefix only.
    dut.expect("HOST_CONNECTED id=", timeout=20)
    dut.expect_exact("HOST_SECURITY encrypted=1 bonded=1", timeout=20)
    dut.expect_exact("HOST_DISCOVERED success=1 report=1", timeout=20)

    dut.write("A")
    dut.expect_exact("HOST_LISTENERS_ADDED key1=1 key2=1 mouse1=1", timeout=10)

    # One keyboard event must reach the primary callback and both listeners.
    device.write("a")
    device.expect_exact("DEVICE_PRESS_KEY success=1", timeout=10)
    dut.expect_exact("HOST_KEY usage=4 ascii=97 pressed=1 released=0 modifiers=0", timeout=20)
    dut.expect_exact("HOST_KEY_L1 usage=4 pressed=1", timeout=20)
    dut.expect_exact("HOST_KEY_L2 usage=4 pressed=1", timeout=20)
    device.write("r")
    device.expect_exact("DEVICE_RELEASE_ALL success=1", timeout=10)

    # A mouse listener coexists with the keyboard ones without cross-talk.
    device.write("o")
    device.expect_exact("DEVICE_WHEEL success=1", timeout=10)
    dut.expect_exact("HOST_MOUSE x=0 y=0 wheel=3 buttons=0 moved=1 changed=0", timeout=20)
    dut.expect_exact("HOST_MOUSE_L1 wheel=3 buttons=0", timeout=20)

    # Removing one listener leaves the primary callback and the other listener.
    dut.write("R")
    dut.expect_exact("HOST_LISTENER_REMOVED success=1", timeout=10)
    dut.write("r")
    dut.expect_exact("HOST_LISTENER_REMOVED_AGAIN success=0", timeout=10)

    device.write("t")
    device.expect_exact("DEVICE_TAP_KEY success=1", timeout=10)
    dut.expect_exact("HOST_KEY usage=5 ascii=98 pressed=1 released=0 modifiers=0", timeout=20)
    dut.expect_exact("HOST_KEY_L1 usage=5 pressed=1", timeout=20)
    dut.expect_exact("HOST_KEY usage=5 ascii=98 pressed=0 released=1 modifiers=0", timeout=20)
    dut.expect_exact("HOST_KEY_L1 usage=5 pressed=0", timeout=20)

    # Capacity is per event and refuses rather than evicting an existing one.
    dut.write("C")
    dut.expect_exact(
        "HOST_LISTENER_CAPACITY total=4 max=4 error=RESOURCE_EXHAUSTED", timeout=10
    )

    device.write("t")
    device.expect_exact("DEVICE_TAP_KEY success=1", timeout=10)
    dut.expect_exact("HOST_KEY usage=5 ascii=98 pressed=1 released=0 modifiers=0", timeout=20)
    dut.expect_exact("HOST_KEY_L1 usage=5 pressed=1", timeout=20)

    dut.write("d")
    dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
    dut.expect("HOST_DISCONNECTED id=", timeout=20)
    device.expect("DEVICE_DISCONNECTED id=", timeout=20)

    dut.write("x")
    device.write("x")
    dut.expect_exact("HOST_BONDS_CLEARED success=1 count=0", timeout=10)
    device.expect_exact("DEVICE_BONDS_CLEARED success=1 count=0", timeout=10)
