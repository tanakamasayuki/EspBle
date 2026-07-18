#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);
  ble.hidMouse().configure();
  EspBleConfig config;
  config.deviceName = "EspBle Mouse";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config)) return;
  ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });
  ble.advertising().setName(config.deviceName);
  ble.advertising().start();
  Serial.println("Send 'm' to move, 'c' to click.");
}

void loop()
{
  if (Serial.available())
  {
    const char command = Serial.read();
    if (command == 'm') ble.hidMouse().move(12, -8);
    else if (command == 'c') ble.hidMouse().click(ESP_BLE_HID_MOUSE_LEFT);
  }
  ble.update();
  delay(1);
}
