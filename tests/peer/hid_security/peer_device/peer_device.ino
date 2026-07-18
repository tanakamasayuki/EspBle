#include <EspBle.h>

// Security-enabled HID Keyboard Device. It must require encryption on the
// HID Service attributes and never push input reports over an unencrypted or
// unsubscribed link.
EspBle ble;
EspBleConnectionId connectionId = 0;

void setup()
{
  Serial.begin(115200);
  delay(500);

  auto &keyboard = ble.hidKeyboard();
  EspBleHidKeyboardConfig keyboardConfig;
  keyboardConfig.manufacturer = "EspBle Security Peer";
  keyboardConfig.vendorId = 0x303a;
  keyboardConfig.productId = 0x4005;
  keyboardConfig.productVersion = 0x0100;
  if (!keyboard.configure(keyboardConfig))
  {
    Serial.printf("DEVICE_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle Security Peer";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.pairOnConnect = false;
  if (!ble.begin(config))
  {
    Serial.printf("DEVICE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("DEVICE_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    Serial.printf("DEVICE_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
    ble.advertising().start();
    Serial.printf("DEVICE_READVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
  });
  ble.advertising().setName("EspBle Security Peer");
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("DEVICE_ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'x')
    {
      const bool cleared = ble.deleteAllBonds();
      Serial.printf("DEVICE_BONDS_CLEARED success=%u count=%u\n",
        cleared ? 1 : 0, static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == 'k')
    {
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      report.keys[0] = 0x04;
      Serial.printf("DEVICE_INPUT_SENT success=%u error=%s\n",
        ble.hidKeyboard().sendReport(report) ? 1 : 0,
        ble.lastErrorName());
    }
    else if (command == 'r')
    {
      Serial.printf("DEVICE_RELEASE_SENT success=%u\n",
        ble.hidKeyboard().releaseAll() ? 1 : 0);
    }
    else if (command == 'd')
    {
      Serial.printf("DEVICE_DISCONNECT_STARTED success=%u\n", ble.disconnect(connectionId) ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
