#include <EspBle.h>

// Central WITHOUT security: it must not be able to read HID data from a
// security-enabled HID Keyboard Device over an unencrypted link.
EspBle ble;
EspBleConnectionId connectionId = 0;
unsigned notificationCount = 0;

void setup()
{
  Serial.begin(115200);
  delay(500);

  ble.hidKeyboardHost().onDiscovered([](const EspBleHidKeyboardHostDiscovery &result) {
    Serial.printf(
      "HOST_DISCOVERED success=%u detail=%s\n",
      result.success ? 1 : 0,
      result.detail.c_str());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Insecure Host";
  if (!ble.begin(config))
  {
    Serial.printf("HOST_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("HOST_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    Serial.printf("HOST_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    Serial.printf(
      "HOST_READ success=%u len=%u\n",
      result.success ? 1 : 0,
      static_cast<unsigned>(result.value.length()));
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("HOST_SUBSCRIBED success=%u\n", result.success ? 1 : 0);
  });
  ble.onNotification([](const EspBleGattNotification &) {
    ++notificationCount;
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService("1812"))
    {
      ble.scanner().stop();
      Serial.printf("HOST_CONNECT_STARTED success=%u\n", ble.connect(result) ? 1 : 0);
    }
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's')
    {
      Serial.printf("HOST_SCAN_STARTED success=%u\n", ble.scanner().start() ? 1 : 0);
    }
    else if (command == 'r')
    {
      Serial.printf(
        "HOST_READ_STARTED success=%u\n",
        ble.readCharacteristic(connectionId, "1812", "2a4b") ? 1 : 0);
    }
    else if (command == 'i')
    {
      Serial.printf(
        "HOST_DISCOVERY_STARTED success=%u\n",
        ble.hidKeyboardHost().discover(connectionId) ? 1 : 0);
    }
    else if (command == 'v')
    {
      Serial.printf(
        "HOST_SUBSCRIBE_STARTED success=%u\n",
        ble.subscribe(connectionId, "1812", "2a4d") ? 1 : 0);
    }
    else if (command == 'q')
    {
      Serial.printf(
        "HOST_QUERY notifications=%u connections=%u\n",
        notificationCount,
        static_cast<unsigned>(ble.connectionCount()));
    }
    else if (command == 'd')
    {
      Serial.printf("HOST_DISCONNECT_STARTED success=%u\n", ble.disconnect(connectionId) ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
