#include <EspBle.h>

EspBle ble;
EspBleConnectionId connectionId = 0;
EspBleConnectionId lastConnectionId = 0;
unsigned connectedCount = 0;
unsigned disconnectedCount = 0;
unsigned stateEventCount = 0;
unsigned aReleaseCount = 0;
bool paused = false;
uint32_t resumeAtMs = 0;

static EspBleConfig makeConfig()
{
  EspBleConfig config;
  config.deviceName = "EspBle Robustness Host";
  return config;
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  ble.hidHost().onDiscovered([](const EspBleHidKeyboardHostDiscovery &result) {
    Serial.printf(
      "HOST_DISCOVERED success=%u detail=%s\n",
      result.success ? 1 : 0,
      result.detail.c_str());
  });
  ble.hidHost().onKeyboardState([](const EspBleHidKeyboardState &state) {
    ++stateEventCount;
    if (state.wasReleased(0x04))
    {
      ++aReleaseCount;
    }
    Serial.printf(
      "HOST_STATE a_down=%u a_released=%u\n",
      state.isDown(0x04) ? 1 : 0,
      state.wasReleased(0x04) ? 1 : 0);
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
    if (command == 'c')
    {
      connectedCount = 0;
      disconnectedCount = 0;
      stateEventCount = 0;
      aReleaseCount = 0;
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
        ble.hidHost().discover(connectionId) ? 1 : 0);
    }
    else if (command == 'D')
    {
      // Disconnecting the same connection while its HID discovery is running
      // must be rejected.
      const bool discovering = ble.hidHost().discover(connectionId);
      const bool disconnected = ble.disconnect(connectionId);
      Serial.printf(
        "HOST_DISCOVER_DISCONNECT discover=%u disconnect=%u error=%s\n",
        discovering ? 1 : 0,
        disconnected ? 1 : 0,
        ble.lastErrorName());
    }
    else if (command == 'g')
    {
      EspBleConfig other;
      other.deviceName = "EspBle Different Name";
      other.preferredMtu = 247;
      Serial.printf(
        "HOST_REBEGIN success=%u error=%s\n",
        ble.begin(other) ? 1 : 0,
        ble.lastErrorName());
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
        "HOST_QUERY connected=%u disconnected=%u states=%u releases=%u connections=%u ready=%u dropped=%u\n",
        connectedCount,
        disconnectedCount,
        stateEventCount,
        aReleaseCount,
        static_cast<unsigned>(ble.connectionCount()),
        ble.hidHost().ready(lastConnectionId) ? 1 : 0,
        static_cast<unsigned>(ble.hidHost().droppedEventCount()));
    }
    else if (command == 'd')
    {
      Serial.printf("HOST_DISCONNECT_STARTED success=%u\n", ble.disconnect(connectionId) ? 1 : 0);
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
