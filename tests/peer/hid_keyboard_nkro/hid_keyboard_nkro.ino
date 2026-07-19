#include <EspBle.h>

EspBle ble;
EspBleConnectionId connectionId = 0;

void setup()
{
  Serial.begin(115200);
  delay(500);
  ble.hidHost().onDiscovered([](const EspBleHidKeyboardHostDiscovery &result) {
    Serial.printf("HOST_DISCOVERED success=%u report=%u output=%u detail=%s\n",
      result.success ? 1 : 0, result.reportId, result.hasOutputReport ? 1 : 0,
      result.detail.c_str());
  });
  ble.hidHost().onKeyboardState([](const EspBleHidKeyboardState &state) {
    unsigned count = 0;
    for (unsigned usage = 0; usage < 256; ++usage)
      if (state.isDown(static_cast<uint8_t>(usage))) ++count;
    Serial.printf("HOST_NKRO_STATE count=%u high=%u b=%u b_released=%u\n",
      count, state.isDown(0x87) ? 1 : 0, state.isDown(0x05) ? 1 : 0,
      state.wasReleased(0x05) ? 1 : 0);
  });

  EspBleConfig config;
  config.deviceName = "EspBle NKRO Host Test";
  config.preferredMtu = 64;
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config)) return;
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("HOST_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    if (event.success)
      Serial.printf("HOST_DISCOVERY_STARTED success=%u\n",
        ble.hidHost().discover(event.connection.id) ? 1 : 0);
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
    if (command == 'x')
      Serial.printf("HOST_BONDS_CLEARED success=%u\n", ble.deleteAllBonds() ? 1 : 0);
    else if (command == 's')
      Serial.printf("HOST_SCAN_STARTED success=%u\n", ble.scanner().start() ? 1 : 0);
    else if (command == 'l')
      Serial.printf("HOST_LEDS_WRITTEN success=%u\n",
        ble.hidHost().setKeyboardLeds(connectionId, true, true, false) ? 1 : 0);
  }
  ble.update();
  delay(1);
}
