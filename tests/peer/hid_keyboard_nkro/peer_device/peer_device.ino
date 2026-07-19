#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);
  auto &keyboard = ble.hidKeyboard();
  keyboard.enableNkro();
  keyboard.configure();
  keyboard.onOutputReport([](const EspBleHidKeyboardOutputReport &report) {
    Serial.printf("DEVICE_OUTPUT leds=%u\n", report.leds);
  });

  EspBleConfig config;
  config.deviceName = "EspBle NKRO Peer";
  config.preferredMtu = 64;
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config)) return;
  ble.advertising().setName("EspBle NKRO Peer");
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'x')
      Serial.printf("DEVICE_BONDS_CLEARED success=%u\n", ble.deleteAllBonds() ? 1 : 0);
    else if (command == 'n')
    {
      const uint8_t usages[] = {0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x87};
      bool success = true;
      for (uint8_t usage : usages)
      {
        success = ble.hidKeyboard().pressUsage(usage) && success;
        delay(25);
      }
      Serial.printf("DEVICE_NKRO_SENT success=%u\n", success ? 1 : 0);
    }
    else if (command == 'b')
      Serial.printf("DEVICE_RELEASE_USAGE success=%u\n",
        ble.hidKeyboard().releaseUsage(0x05) ? 1 : 0);
    else if (command == 'r')
      Serial.printf("DEVICE_RELEASE_ALL success=%u\n",
        ble.hidKeyboard().releaseAll() ? 1 : 0);
  }
  ble.update();
  delay(1);
}
