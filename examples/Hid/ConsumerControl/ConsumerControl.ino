#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);
  ble.hidConsumerControl().configure();
  EspBleConfig config;
  config.deviceName = "EspBle Media Keys";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config)) return;
  ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });
  ble.advertising().setName(config.deviceName);
  ble.advertising().start();
  Serial.println("Send '+', '-', or 'p'.");
}

void loop()
{
  if (Serial.available())
  {
    const char command = Serial.read();
    if (command == '+') ble.hidConsumerControl().click(ESP_BLE_HID_CONSUMER_CONTROL_VOLUME_UP);
    else if (command == '-') ble.hidConsumerControl().click(ESP_BLE_HID_CONSUMER_CONTROL_VOLUME_DOWN);
    else if (command == 'p') ble.hidConsumerControl().click(ESP_BLE_HID_CONSUMER_CONTROL_PLAY_PAUSE);
  }
  ble.update();
  delay(1);
}
