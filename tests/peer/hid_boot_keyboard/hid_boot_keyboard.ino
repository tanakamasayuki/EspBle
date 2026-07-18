#include <EspBle.h>

EspBle ble;
EspBleConnectionId connectionId = 0;
EspBleConnectionId lastConnectionId = 0;
unsigned stateEventCount = 0;
unsigned aReleaseCount = 0;

void setup()
{
  Serial.begin(115200);
  delay(500);

  ble.hidHost().onDiscovered([](const EspBleHidKeyboardHostDiscovery &result) {
    Serial.printf(
      "HOST_DISCOVERED success=%u report=%u output=%u battery=%u detail=%s\n",
      result.success ? 1 : 0,
      result.reportId,
      result.hasOutputReport ? 1 : 0,
      result.hasBatteryLevel ? 1 : 0,
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

  EspBleConfig config;
  config.deviceName = "EspBle Boot Host";
  if (!ble.begin(config))
  {
    Serial.printf("HOST_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    lastConnectionId = connection.id;
    Serial.printf("HOST_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
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
    else if (command == 'q')
    {
      Serial.printf(
        "HOST_QUERY states=%u releases=%u connections=%u ready=%u invalid=%u\n",
        stateEventCount,
        aReleaseCount,
        static_cast<unsigned>(ble.connectionCount()),
        ble.hidHost().ready(lastConnectionId) ? 1 : 0,
        static_cast<unsigned>(ble.hidHost().invalidInputReportCount()));
    }
    else if (command == 'd')
    {
      Serial.printf("HOST_DISCONNECT_STARTED success=%u\n", ble.disconnect(connectionId) ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
