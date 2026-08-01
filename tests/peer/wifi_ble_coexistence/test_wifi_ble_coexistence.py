def test_wifi_ble_coexistence_and_shared_transport_lifecycle(dut, peers):
    peripheral = peers["device"]

    peripheral.write("?")
    peripheral.expect_exact("ADVERTISING 1", timeout=10)

    # Wi-Fi owns the ESP-Hosted transport first.
    dut.write("w")
    dut.expect_exact(
        "WIFI_STARTED wifi_connected=1 ip=1 hosted=1 wifi=1 ble=0",
        timeout=40,
    )

    # Starting BLE must share that transport, while ordinary GATT traffic and
    # notifications continue to work with Wi-Fi connected.
    dut.write("b")
    dut.expect_exact(
        "BLE_STARTED wifi_connected=1 ip=1 hosted=1 wifi=1 ble=1",
        timeout=20,
    )
    dut.write("s")
    dut.expect_exact("SCAN_STARTED", timeout=10)
    dut.expect_exact("CONNECT_REQUESTED", timeout=30)
    dut.expect_exact("DISCOVER_REQUESTED", timeout=20)
    dut.expect_exact("DISCOVER success=1 read=1 write=1 notify=1 wifi=1", timeout=20)
    dut.expect_exact("READ_REQUESTED", timeout=10)
    dut.expect_exact("READ success=1 value=s3-ready wifi=1", timeout=20)
    dut.expect_exact("WRITE_REQUESTED", timeout=10)
    dut.expect_exact("WRITE success=1 wifi=1", timeout=20)
    dut.expect_exact("SUBSCRIBE_REQUESTED", timeout=10)
    dut.expect_exact(
        "SUBSCRIBED success=1 wifi=1 hosted=1 wifi_active=1 ble_active=1",
        timeout=20,
    )
    peripheral.expect_exact("SERVER_WRITE value=p4-write", timeout=20)
    peripheral.expect_exact("SERVER_SUBSCRIBED 1", timeout=20)
    peripheral.write("n")
    peripheral.expect_exact("NOTIFY_ACCEPTED 1", timeout=10)
    dut.expect_exact("NOTIFICATION value=s3-notify wifi=1", timeout=20)

    # EspBle owns only the BLE reference. Its teardown must leave the transport
    # and Wi-Fi data path alive until Wi-Fi releases the final reference.
    dut.write("e")
    dut.expect_exact(
        "BLE_ENDED wifi_connected=1 ip=1 hosted=1 wifi=1 ble=0",
        timeout=20,
    )
    dut.write("q")
    dut.expect_exact("WIFI_END_RESULT 1", timeout=10)
    dut.expect_exact(
        "WIFI_ENDED wifi_connected=0 ip=0 hosted=0 wifi=0 ble=0",
        timeout=20,
    )
