#include <EspBle.h>

EspBle ble;
EspBleConnectionId connectionId = 0;

void setup()
{
  Serial.begin(115200);
  delay(500);

  auto &keyboard = ble.hidKeyboard();
  EspBleHidKeyboardConfig keyboardConfig;
  keyboardConfig.manufacturer = "EspBle Robustness Peer";
  keyboardConfig.vendorId = 0x303a;
  keyboardConfig.productId = 0x4004;
  keyboardConfig.productVersion = 0x0100;
  if (!keyboard.configure(keyboardConfig))
  {
    Serial.printf("DEVICE_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle Robustness Peer";
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
  ble.advertising().setName("EspBle Robustness Peer");
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
    else if (command == 'k')
    {
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      report.keys[0] = 0x04;
      Serial.printf("DEVICE_INPUT_SENT success=%u error=%s\n",
        ble.hidKeyboard().sendReport(report) ? 1 : 0,
        ble.lastErrorName());
    }
    else if (command == 'o')
    {
      // Phantom state (ErrorRollOver): all six key slots report usage 0x01.
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      for (uint8_t index = 0; index < 6; ++index)
      {
        report.keys[index] = 0x01;
      }
      Serial.printf("DEVICE_ROLLOVER_SENT success=%u\n",
        ble.hidKeyboard().sendReport(report) ? 1 : 0);
    }
    else if (command == 'f')
    {
      // Flood the host's HID event queue with state changes while it is not
      // dispatching: toggle the B key 9 times with Shift+A held throughout.
      unsigned sent = 0;
      for (uint8_t index = 0; index < 9; ++index)
      {
        EspBleHidKeyboardInputReport report;
        report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
        report.keys[0] = 0x04;
        if ((index & 1) == 0)
        {
          report.keys[1] = 0x05;
        }
        if (ble.hidKeyboard().sendReport(report))
        {
          ++sent;
        }
        delay(20);
      }
      Serial.printf("DEVICE_FLOOD_SENT sent=%u\n", sent);
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
