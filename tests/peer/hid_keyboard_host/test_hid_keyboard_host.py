import re


def test_hid_keyboard_host_discovery_state_and_leds(dut, peers):
    keyboard_device = peers["device"]

    dut.write("x")
    keyboard_device.write("x")
    dut.expect_exact("HOST_BONDS_CLEARED success=1 count=0", timeout=10)
    keyboard_device.expect_exact("DEVICE_BONDS_CLEARED success=1 count=0", timeout=10)

    keyboard_device.write("?")
    keyboard_device.expect_exact("DEVICE_ADVERTISING 1", timeout=10)
    dut.write("s")
    dut.expect_exact("HOST_SCAN_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_CONNECT_STARTED success=1", timeout=20)
    dut.expect_exact("HOST_CONNECTED id=1", timeout=20)
    keyboard_device.expect_exact("DEVICE_CONNECTED id=1", timeout=20)
    dut.expect_exact("HOST_SECURITY encrypted=1 bonded=1 context=loop", timeout=20)
    keyboard_device.expect_exact(
        "DEVICE_SECURITY encrypted=1 bonded=1 context=loop", timeout=20
    )
    dut.expect_exact("HOST_DISCOVERY_STARTED success=1", timeout=10)
    dut.expect_exact(
        "HOST_DISCOVERED success=1 report=1 country_present=1 country=33 output=1 battery_present=1 battery=73 context=loop detail=",
        timeout=20,
    )

    keyboard_device.write("o")
    keyboard_device.expect_exact("DEVICE_MOUSE_SENT success=1", timeout=10)
    dut.expect_exact(
        "HOST_MOUSE x=-9 y=6 wheel=-1 buttons=2 moved=1 changed=1 context=loop",
        timeout=20,
    )

    keyboard_device.write("c")
    keyboard_device.expect_exact("DEVICE_CONSUMER_SENT success=1", timeout=10)
    dut.expect_exact("HOST_CONSUMER usage=205 pressed=1 released=0 context=loop", timeout=20)
    keyboard_device.write("u")
    keyboard_device.expect_exact("DEVICE_CONSUMER_RELEASED success=1", timeout=10)
    dut.expect_exact("HOST_CONSUMER usage=0 pressed=0 released=1 context=loop", timeout=20)

    keyboard_device.write("s")
    keyboard_device.expect_exact("DEVICE_SYSTEM_SENT success=1", timeout=10)
    dut.expect_exact("HOST_SYSTEM usage=3 pressed=1 released=0 context=loop", timeout=20)

    keyboard_device.write("p")
    keyboard_device.expect_exact("DEVICE_GAMEPAD_SENT success=1", timeout=10)
    dut.expect_exact("HOST_GAMEPAD fields=8 changed=1 x=10 context=loop", timeout=20)

    dut.write("e")
    dut.expect_exact("HOST_LAYOUT en-US", timeout=10)
    keyboard_device.write("t")
    keyboard_device.expect_exact("DEVICE_LAYOUT_KEY_SENT success=1", timeout=10)
    dut.expect_exact(
        "HOST_KEY usage=31 ascii=64 pressed=1 released=0 modifiers=2 context=loop",
        timeout=20,
    )
    keyboard_device.write("r")
    keyboard_device.expect_exact("DEVICE_RELEASE_SENT success=1", timeout=10)

    keyboard_device.write("m")
    keyboard_device.expect_exact("DEVICE_MODIFIER_SENT success=1", timeout=10)
    dut.expect_exact(
        "HOST_KEY usage=225 ascii=0 pressed=1 released=0 modifiers=2 context=loop",
        timeout=20,
    )
    keyboard_device.write("r")
    keyboard_device.expect_exact("DEVICE_RELEASE_SENT success=1", timeout=10)
    dut.expect_exact(
        "HOST_KEY usage=225 ascii=0 pressed=0 released=1 modifiers=0 context=loop",
        timeout=20,
    )

    dut.write("j")
    dut.expect_exact("HOST_LAYOUT ja-JP", timeout=10)
    keyboard_device.write("t")
    keyboard_device.expect_exact("DEVICE_LAYOUT_KEY_SENT success=1", timeout=10)
    dut.expect_exact(
        "HOST_KEY usage=31 ascii=34 pressed=1 released=0 modifiers=2 context=loop",
        timeout=20,
    )
    keyboard_device.write("r")
    keyboard_device.expect_exact("DEVICE_RELEASE_SENT success=1", timeout=10)

    dut.write("g")
    dut.expect_exact("HOST_LAYOUT de-DE", timeout=10)
    keyboard_device.write("y")
    keyboard_device.expect_exact("DEVICE_Y_POSITION_SENT success=1", timeout=10)
    dut.expect_exact(
        "HOST_KEY usage=28 ascii=122 pressed=1 released=0 modifiers=0 context=loop",
        timeout=20,
    )
    keyboard_device.write("r")
    keyboard_device.expect_exact("DEVICE_RELEASE_SENT success=1", timeout=10)

    dut.write("f")
    dut.expect_exact("HOST_LAYOUT fr-FR", timeout=10)
    keyboard_device.write("a")
    keyboard_device.expect_exact("DEVICE_A_POSITION_SENT success=1", timeout=10)
    dut.expect_exact(
        "HOST_KEY usage=4 ascii=113 pressed=1 released=0 modifiers=0 context=loop",
        timeout=20,
    )
    keyboard_device.write("r")
    keyboard_device.expect_exact("DEVICE_RELEASE_SENT success=1", timeout=10)

    dut.write("b")
    dut.expect_exact("HOST_LAYOUT en-GB", timeout=10)
    keyboard_device.write("t")
    keyboard_device.expect_exact("DEVICE_LAYOUT_KEY_SENT success=1", timeout=10)
    dut.expect_exact(
        "HOST_KEY usage=31 ascii=34 pressed=1 released=0 modifiers=2 context=loop",
        timeout=20,
    )
    keyboard_device.write("r")
    keyboard_device.expect_exact("DEVICE_RELEASE_SENT success=1", timeout=10)

    keyboard_device.write("k")
    keyboard_device.expect_exact("DEVICE_INPUT_SENT success=1", timeout=10)
    dut.expect_exact(
        "HOST_STATE id=1 modifiers=2 a_down=1 a_pressed=1 a_released=0 context=loop",
        timeout=20,
    )

    keyboard_device.write("r")
    keyboard_device.expect_exact("DEVICE_RELEASE_SENT success=1", timeout=10)
    dut.expect_exact(
        "HOST_STATE id=1 modifiers=0 a_down=0 a_pressed=0 a_released=1 context=loop",
        timeout=20,
    )

    dut.write("l")
    dut.expect_exact("HOST_LEDS_WRITTEN success=1", timeout=20)
    keyboard_device.expect_exact(
        "DEVICE_OUTPUT leds=3 num=1 caps=1 context=loop", timeout=20
    )

    keyboard_device.write("k")
    keyboard_device.expect_exact("DEVICE_INPUT_SENT success=1", timeout=10)
    dut.expect_exact(
        "HOST_STATE id=1 modifiers=2 a_down=1 a_pressed=1 a_released=0 context=loop",
        timeout=20,
    )

    # LED書き込みはWrite Without Responseによるfire-and-forgetで、
    # ATT応答を待って呼び出しtaskをblockしない(10回+5ms間隔で200ms未満)。
    dut.write("L")
    match = dut.expect(
        re.compile(rb"HOST_LEDS_TIMED success=(\d+) ms=(\d+)"), timeout=20
    )
    led_success = int(match.group(1))
    led_ms = int(match.group(2))
    assert led_success == 10, f"expected 10 LED writes to succeed, got {led_success}"
    assert led_ms < 200, f"LED writes blocked on ATT responses: {led_ms}ms for 10 writes"
    keyboard_device.expect_exact(
        "DEVICE_OUTPUT leds=3 num=1 caps=1 context=loop", timeout=20
    )

    dut.write("d")
    dut.expect_exact("HOST_DISCONNECT_STARTED success=1", timeout=10)
    dut.expect_exact("HOST_DISCONNECTED id=1 context=loop", timeout=20)
    dut.expect_exact(
        "HOST_STATE id=1 modifiers=0 a_down=0 a_pressed=0 a_released=1 context=loop",
        timeout=20,
    )
    keyboard_device.expect_exact("DEVICE_DISCONNECTED id=1 context=loop", timeout=20)

    dut.write("x")
    keyboard_device.write("x")
    dut.expect_exact("HOST_BONDS_CLEARED success=1 count=0", timeout=10)
    keyboard_device.expect_exact("DEVICE_BONDS_CLEARED success=1 count=0", timeout=10)
