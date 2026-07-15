#include <EspBle.h>

EspBle ble;
EspBleConnectionId keyboardConnectionId = 0;

void setup()
{
  Serial.begin(115200);

  auto &keyboard = ble.hidKeyboardHost();
  keyboard.onDiscovered([](const EspBleHidKeyboardHostDiscovery &result) {
    if (!result.success)
    {
      Serial.printf("Keyboard discovery failed: %s\n", result.detail.c_str());
      return;
    }
    Serial.printf(
      "Keyboard ready: report=%u battery=%s%u%%\n",
      result.reportId,
      result.hasBatteryLevel ? "" : "unknown/",
      result.batteryLevel);
  });
  keyboard.onKeyboardState([](const EspBleHidKeyboardState &state) {
    Serial.printf(
      "Keyboard state: modifiers=0x%02x A=%u pressed=%u released=%u\n",
      state.modifiers,
      state.isDown(0x04) ? 1 : 0,
      state.wasPressed(0x04) ? 1 : 0,
      state.wasReleased(0x04) ? 1 : 0);
  });
  keyboard.setKeyboardLayout(EspBleKeyboardLayout::EnUs);
  keyboard.onKeyboard([](const EspBleHidKeyboardEvent &event) {
    if (event.pressed)
    {
      Serial.printf("Key pressed: usage=0x%02x ascii=0x%02x\n", event.usage, event.ascii);
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Keyboard Host";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    keyboardConnectionId = connection.id;
    ble.hidKeyboardHost().discover(connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &) {
    keyboardConnectionId = 0;
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService("1812"))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  if (Serial.available() > 0 && keyboardConnectionId != 0)
  {
    const char command = Serial.read();
    if (command == 'c')
    {
      ble.hidKeyboardHost().setKeyboardLeds(
        keyboardConnectionId, false, true, false);
    }
    else if (command == '0')
    {
      ble.hidKeyboardHost().setKeyboardLeds(
        keyboardConnectionId, false, false, false);
    }
  }

  ble.update();
  delay(1);
}
