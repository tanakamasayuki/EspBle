#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleHidKeyboardDeviceConfig keyboardConfig;
  keyboardConfig.manufacturer = "EspBle KeyBridge Peer";
  keyboardConfig.vendorId = 0x303a;
  keyboardConfig.productId = 0x4003;
  if (!ble.hidKeyboardDevice().configure(keyboardConfig))
  {
    Serial.printf("DEVICE_CONFIG_FAILED %s\n", ble.lastErrorName());
    return;
  }
  ble.hidKeyboardDevice().onOutputReport(
    [](const EspBleHidKeyboardOutputReport &report) {
      Serial.printf("DEVICE_LEDS num=%u caps=%u scroll=%u\n",
        report.numLock() ? 1 : 0,
        report.capsLock() ? 1 : 0,
        report.scrollLock() ? 1 : 0);
    });

  EspBleConfig config;
  config.deviceName = "EspBle KeyBridge Peer";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("DEVICE_INIT_FAILED %s\n", ble.lastErrorName());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("DEVICE_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf("DEVICE_SECURITY success=%u bonded=%u\n",
      event.success ? 1 : 0, event.connection.bonded ? 1 : 0);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("DEVICE_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
    ble.advertising().start();
  });
  ble.advertising().setName("EspBle KeyBridge Peer");
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'x')
    {
      Serial.printf("DEVICE_BONDS_CLEARED success=%u count=%u\n",
        ble.deleteAllBonds() ? 1 : 0, static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == '?')
    {
      Serial.printf("DEVICE_ADVERTISING %u\n",
        ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'k')
    {
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      report.keys[0] = 0x04;
      Serial.printf("DEVICE_SHIFT_A_SENT success=%u\n",
        ble.hidKeyboardDevice().sendInputReport(report) ? 1 : 0);
    }
    else if (command == 'c')
    {
      EspBleHidKeyboardInputReport report;
      report.keys[0] = 0x39;
      Serial.printf("DEVICE_CAPS_SENT success=%u\n",
        ble.hidKeyboardDevice().sendInputReport(report) ? 1 : 0);
    }
    else if (command == 'r')
    {
      Serial.printf("DEVICE_RELEASE_SENT success=%u\n",
        ble.hidKeyboardDevice().releaseAll() ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
