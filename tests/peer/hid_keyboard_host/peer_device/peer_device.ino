#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

EspBle ble;
TaskHandle_t loopTask = nullptr;

static const char *callbackContext()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  auto &keyboard = ble.hidKeyboard();
  EspBleHidKeyboardConfig keyboardConfig;
  keyboardConfig.manufacturer = "EspBle Host Peer";
  keyboardConfig.vendorId = 0x303a;
  keyboardConfig.productId = 0x4002;
  keyboardConfig.productVersion = 0x0100;
  keyboardConfig.countryCode = 33;
  keyboardConfig.initialBatteryLevel = 73;
  if (!keyboard.configure(keyboardConfig))
  {
    Serial.printf("DEVICE_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  if (!ble.hidMouse().configure())
  {
    Serial.printf("DEVICE_MOUSE_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  if (!ble.hidConsumerControl().configure() || !ble.hidSystemControl().configure() ||
      !ble.hidGamepad().configure())
  {
    Serial.printf("DEVICE_COMPOSITE_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  keyboard.onOutputReport([](const EspBleHidKeyboardOutputReport &report) {
    Serial.printf(
      "DEVICE_OUTPUT leds=%u num=%u caps=%u context=%s\n",
      report.leds,
      report.numLock() ? 1 : 0,
      report.capsLock() ? 1 : 0,
      callbackContext());
  });

  EspBleConfig config;
  config.deviceName = "EspBle HID Host Peer";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("DEVICE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("DEVICE_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "DEVICE_SECURITY encrypted=%u bonded=%u context=%s\n",
      event.connection.encrypted ? 1 : 0,
      event.connection.bonded ? 1 : 0,
      callbackContext());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("DEVICE_DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), callbackContext());
  });
  ble.advertising().setName("EspBle HID Host Peer");
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'x')
    {
      const bool cleared = ble.deleteAllBonds();
      Serial.printf("DEVICE_BONDS_CLEARED success=%u count=%u\n",
        cleared ? 1 : 0, static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == '?')
    {
      Serial.printf("DEVICE_ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'k')
    {
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      report.keys[0] = 0x04;
      Serial.printf("DEVICE_INPUT_SENT success=%u\n",
        ble.hidKeyboard().sendReport(report) ? 1 : 0);
    }
    else if (command == 'r')
    {
      Serial.printf("DEVICE_RELEASE_SENT success=%u\n",
        ble.hidKeyboard().releaseAll() ? 1 : 0);
    }
    else if (command == 't')
    {
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      report.keys[0] = 0x1f; // Keyboard 2 and @ on en-US, " on ja-JP.
      Serial.printf("DEVICE_LAYOUT_KEY_SENT success=%u\n",
        ble.hidKeyboard().sendReport(report) ? 1 : 0);
    }
    else if (command == 'm')
    {
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      Serial.printf("DEVICE_MODIFIER_SENT success=%u\n",
        ble.hidKeyboard().sendReport(report) ? 1 : 0);
    }
    else if (command == 'y')
    {
      EspBleHidKeyboardInputReport report;
      report.keys[0] = 0x1c; // Y on QWERTY, Z on QWERTZ.
      Serial.printf("DEVICE_Y_POSITION_SENT success=%u\n",
        ble.hidKeyboard().sendReport(report) ? 1 : 0);
    }
    else if (command == 'a')
    {
      EspBleHidKeyboardInputReport report;
      report.keys[0] = 0x04; // A on QWERTY, Q on AZERTY.
      Serial.printf("DEVICE_A_POSITION_SENT success=%u\n",
        ble.hidKeyboard().sendReport(report) ? 1 : 0);
    }
    else if (command == 'o')
    {
      Serial.printf("DEVICE_MOUSE_SENT success=%u\n",
        ble.hidMouse().move(-9, 6, -1, ESP_BLE_HID_MOUSE_RIGHT) ? 1 : 0);
    }
    else if (command == 'c')
    {
      Serial.printf("DEVICE_CONSUMER_SENT success=%u\n",
        ble.hidConsumerControl().press(ESP_BLE_HID_CONSUMER_CONTROL_PLAY_PAUSE) ? 1 : 0);
    }
    else if (command == 'u')
    {
      Serial.printf("DEVICE_CONSUMER_RELEASED success=%u\n",
        ble.hidConsumerControl().release() ? 1 : 0);
    }
    else if (command == 's')
    {
      Serial.printf("DEVICE_SYSTEM_SENT success=%u\n",
        ble.hidSystemControl().press(ESP_BLE_HID_SYSTEM_CONTROL_WAKE_HOST) ? 1 : 0);
    }
    else if (command == 'p')
    {
      Serial.printf("DEVICE_GAMEPAD_SENT success=%u\n",
        ble.hidGamepad().send(10, -20, 0, 0, 0, 0, ESP_BLE_HID_GAMEPAD_HAT_UP, 3) ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
