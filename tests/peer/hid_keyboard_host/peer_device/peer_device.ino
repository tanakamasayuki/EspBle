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

  auto &keyboard = ble.hidKeyboardDevice();
  EspBleHidKeyboardDeviceConfig keyboardConfig;
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
        ble.hidKeyboardDevice().sendInputReport(report) ? 1 : 0);
    }
    else if (command == 'r')
    {
      Serial.printf("DEVICE_RELEASE_SENT success=%u\n",
        ble.hidKeyboardDevice().releaseAll() ? 1 : 0);
    }
    else if (command == 't')
    {
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      report.keys[0] = 0x1f; // Keyboard 2 and @ on en-US, " on ja-JP.
      Serial.printf("DEVICE_LAYOUT_KEY_SENT success=%u\n",
        ble.hidKeyboardDevice().sendInputReport(report) ? 1 : 0);
    }
    else if (command == 'm')
    {
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      Serial.printf("DEVICE_MODIFIER_SENT success=%u\n",
        ble.hidKeyboardDevice().sendInputReport(report) ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
