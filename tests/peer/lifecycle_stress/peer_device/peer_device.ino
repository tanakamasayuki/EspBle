#include <EspBle.h>
#include <esp_bt.h>

EspBle ble;
EspBleConnectionId connectionId = 0;
bool radioKilled = false;

void setup()
{
  Serial.begin(115200);
  delay(500);

  auto &keyboard = ble.hidKeyboard();
  EspBleHidKeyboardConfig keyboardConfig;
  keyboardConfig.manufacturer = "EspBle Lifecycle Peer";
  keyboardConfig.vendorId = 0x303a;
  keyboardConfig.productId = 0x4003;
  keyboardConfig.productVersion = 0x0100;
  keyboardConfig.initialBatteryLevel = 80;
  if (!keyboard.configure(keyboardConfig))
  {
    Serial.printf("DEVICE_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle Lifecycle Peer";
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
  ble.advertising().setName("EspBle Lifecycle Peer");
  ble.advertising().start();
  Serial.println("DEVICE_READY");
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
    else if (command == 'B')
    {
      // Burst enough Battery Level notifications to overflow the host's
      // 8-slot connection event queue while its update() loop is paused.
      unsigned sent = 0;
      for (uint8_t index = 0; index < 10; ++index)
      {
        if (ble.hidKeyboard().setBatteryLevel(40 + index))
        {
          ++sent;
        }
        delay(20);
      }
      Serial.printf("DEVICE_BATTERY_BURST sent=%u\n", sent);
    }
    else if (command == 'd')
    {
      Serial.printf("DEVICE_DISCONNECT_STARTED success=%u\n", ble.disconnect(connectionId) ? 1 : 0);
    }
    else if (command == 'X')
    {
      // Simulate silent peer loss: kill the controller without a Link Layer
      // terminate so the central only notices via supervision timeout. The
      // BLE stack on this board is unusable afterwards; the test module ends
      // here and the next module reflashes the board.
      Serial.println("DEVICE_RADIO_KILLED");
      Serial.flush();
      radioKilled = true;
      esp_bt_controller_disable();
    }
  }

  if (!radioKilled)
  {
    ble.update();
  }
  delay(1);
}
