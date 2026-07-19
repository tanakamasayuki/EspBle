#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;

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
  keyboardConfig.manufacturer = "EspBle Test";
  keyboardConfig.vendorId = 0x303a;
  keyboardConfig.productId = 0x4001;
  keyboardConfig.productVersion = 0x0100;
  keyboardConfig.initialBatteryLevel = 87;
  if (!keyboard.configure(keyboardConfig))
  {
    Serial.printf("HID_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  EspBleHidMouseConfig mouseConfig;
  mouseConfig.buttons = 3;
  if (!ble.hidMouse().configure(mouseConfig))
  {
    Serial.printf("MOUSE_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  keyboard.onOutputReport([](const EspBleHidKeyboardOutputReport &report) {
    Serial.printf(
      "OUTPUT_REPORT id=%u leds=%u num=%u caps=%u context=%s\n",
      static_cast<unsigned>(report.connectionId),
      report.leds,
      report.numLock() ? 1 : 0,
      report.capsLock() ? 1 : 0,
      callbackContext());
  });

  EspBleConfig config;
  config.deviceName = "EspBle HID Test";
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("HID_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "HID_SECURITY encrypted=%u bonded=%u context=%s\n",
      event.connection.encrypted ? 1 : 0,
      event.connection.bonded ? 1 : 0,
      callbackContext());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("HID_DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), callbackContext());
    connectionId = 0;
  });

  ble.advertising().setName("EspBle HID Test");
  if (!ble.advertising().start())
  {
    Serial.printf("ADVERTISING_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'x')
    {
      const bool cleared = ble.deleteAllBonds();
      Serial.printf("HID_BONDS_CLEARED success=%u count=%u\n",
        cleared ? 1 : 0, static_cast<unsigned>(ble.bondCount()));
    }
    else if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'k')
    {
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      report.keys[0] = 0x04;
      Serial.println(
        ble.hidKeyboard().sendReport(report) ? "INPUT_SENT" : "INPUT_SEND_FAILED");
    }
    else if (command == 'r')
    {
      Serial.println(
        ble.hidKeyboard().releaseAll() ? "RELEASE_SENT" : "RELEASE_SEND_FAILED");
    }
    else if (command == 'm')
    {
      Serial.println(ble.hidMouse().move(12, -7, 1, ESP_BLE_HID_MOUSE_LEFT)
        ? "MOUSE_SENT" : "MOUSE_SEND_FAILED");
    }
  }

  ble.update();
  delay(1);
}
