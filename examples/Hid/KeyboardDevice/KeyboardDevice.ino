#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  auto &keyboard = ble.hidKeyboardDevice();
  EspBleHidKeyboardDeviceConfig keyboardConfig;
  keyboardConfig.manufacturer = "EspBle";
  keyboard.configure(keyboardConfig);
  keyboard.onOutputReport([](const EspBleHidKeyboardOutputReport &report) {
    Serial.printf(
      "Keyboard LEDs: num=%u caps=%u scroll=%u\n",
      report.numLock() ? 1 : 0,
      report.capsLock() ? 1 : 0,
      report.scrollLock() ? 1 : 0);
  });

  EspBleConfig config;
  config.deviceName = "EspBle Keyboard";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onDisconnected([](const EspBleConnection &) {
    ble.advertising().start();
  });
  ble.advertising().setName("EspBle Keyboard");
  ble.advertising().start();

  Serial.println("Send 'a' to type Shift+A and 'r' to release all keys.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    auto &keyboard = ble.hidKeyboardDevice();
    if (command == 'a')
    {
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      report.keys[0] = 0x04; // Keyboard a and A
      keyboard.sendInputReport(report);
    }
    else if (command == 'r')
    {
      keyboard.releaseAll();
    }
  }

  ble.update();
  delay(1);
}
