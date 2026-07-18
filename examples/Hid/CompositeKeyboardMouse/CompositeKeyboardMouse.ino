#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);
  ble.hidKeyboard().configure();
  ble.hidMouse().configure();
  EspBleConfig config;
  config.deviceName = "EspBle Composite HID";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config)) return;
  ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });
  ble.advertising().setName(config.deviceName);
  ble.advertising().start();
  Serial.println("Send 'h' to type hello, 'm' to move.");
}

void loop()
{
  if (Serial.available())
  {
    const char command = Serial.read();
    if (command == 'h') ble.hidKeyboard().write("hello");
    else if (command == 'm') ble.hidMouse().move(10, 10);
  }
  ble.update();
  delay(1);
}
