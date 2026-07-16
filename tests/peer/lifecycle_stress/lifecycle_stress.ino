#include <EspBle.h>

EspBle ble;
EspBleConnectionId connectionId = 0;
EspBleConnectionId lastConnectionId = 0;
unsigned connectedCount = 0;
unsigned disconnectedCount = 0;
unsigned notificationCount = 0;
bool paused = false;
uint32_t resumeAtMs = 0;

static EspBleConfig makeConfig()
{
  EspBleConfig config;
  config.deviceName = "EspBle Lifecycle Host";
  return config;
}

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

  if (!ble.begin(makeConfig()))
  {
    Serial.printf("HOST_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    lastConnectionId = connection.id;
    ++connectedCount;
    Serial.printf("HOST_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    ++disconnectedCount;
    connectionId = 0;
    Serial.printf("HOST_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf(
      "HOST_SUBSCRIBED success=%u detail=%s\n",
      result.success ? 1 : 0,
      result.detail.c_str());
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
  Serial.println("HOST_READY");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'c')
    {
      connectedCount = 0;
      disconnectedCount = 0;
      notificationCount = 0;
      Serial.println("HOST_COUNTERS_RESET");
    }
    else if (command == 's')
    {
      Serial.printf("HOST_SCAN_STARTED success=%u\n", ble.scanner().start() ? 1 : 0);
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
        ble.subscribe(connectionId, "180f", "2a19") ? 1 : 0);
    }
    else if (command == 'p')
    {
      paused = true;
      resumeAtMs = millis() + 5000;
      Serial.println("HOST_PAUSED");
    }
    else if (command == 'q')
    {
      Serial.printf(
        "HOST_QUERY connected=%u disconnected=%u notifications=%u connections=%u ready=%u dropped=%u\n",
        connectedCount,
        disconnectedCount,
        notificationCount,
        static_cast<unsigned>(ble.connectionCount()),
        ble.hidKeyboardHost().ready(lastConnectionId) ? 1 : 0,
        static_cast<unsigned>(ble.droppedEventCount()));
    }
    else if (command == 'l')
    {
      Serial.printf(
        "HOST_LEDS success=%u\n",
        ble.hidKeyboardHost().setKeyboardLeds(lastConnectionId, true, true, false) ? 1 : 0);
    }
    else if (command == 'd')
    {
      Serial.printf("HOST_DISCONNECT_STARTED success=%u\n", ble.disconnect(connectionId) ? 1 : 0);
    }
    else if (command == 'h')
    {
      Serial.printf("HOST_HEAP free=%u\n", static_cast<unsigned>(ESP.getFreeHeap()));
    }
    else if (command == 'z')
    {
      // Issue an asynchronous GATT read and immediately tear the stack down so
      // end() overlaps the worker task completion, then bring it back up.
      const bool readStarted = ble.readCharacteristic(connectionId, "180a", "2a29");
      ble.end();
      const bool restarted = ble.begin(makeConfig());
      connectionId = 0;
      lastConnectionId = 0;
      Serial.printf(
        "HOST_END_CYCLE read=%u begin=%u heap=%u\n",
        readStarted ? 1 : 0,
        restarted ? 1 : 0,
        static_cast<unsigned>(ESP.getFreeHeap()));
    }
  }

  if (paused)
  {
    if (static_cast<int32_t>(millis() - resumeAtMs) >= 0)
    {
      paused = false;
      Serial.println("HOST_RESUMED");
    }
  }
  else
  {
    ble.update();
  }
  delay(1);
}
