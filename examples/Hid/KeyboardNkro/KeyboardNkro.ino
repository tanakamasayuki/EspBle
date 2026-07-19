#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  auto &keyboard = ble.hidKeyboard();
  keyboard.enableNkro();
  keyboard.configure();

  EspBleConfig config;
  config.deviceName = "EspBle NKRO Keyboard";
  config.preferredMtu = 64; // A 29-byte NKRO Input Report needs MTU >= 32.
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });
  ble.advertising().setName("EspBle NKRO Keyboard");
  ble.advertising().start();
  Serial.println("Send 'n' for eight simultaneous keys, 'r' to release all.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    auto &keyboard = ble.hidKeyboard();
    if (command == 'n')
    {
      const uint8_t usages[] = {0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};
      for (uint8_t usage : usages) keyboard.pressUsage(usage);
    }
    else if (command == 'r') keyboard.releaseAll();
  }
  ble.update();
  delay(1);
}
