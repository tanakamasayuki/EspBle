// hid_boot_protocol peer_device: an EspBle HID keyboard peripheral (security
// disabled so a generic GATT client can drive it directly). It advertises the
// HID service, reports Protocol Mode switches via onProtocolMode(), and sends a
// keyboard report on command. In Boot Protocol Mode the report travels over the
// Boot Keyboard Input Report; LED writes arrive through onOutputReport().
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

EspBle ble;
TaskHandle_t loopTask = nullptr;

static const char *contextName()
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
  keyboardConfig.productId = 0x4002;
  keyboardConfig.productVersion = 0x0100;
  keyboardConfig.initialBatteryLevel = 90;
  keyboardConfig.bootProtocol = true; // expose Protocol Mode + Boot Keyboard reports
  if (!keyboard.configure(keyboardConfig))
  {
    Serial.printf("HID_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  keyboard.onProtocolMode([](uint8_t mode, EspBleConnectionId connectionId) {
    Serial.printf("PROTOCOL_MODE mode=%u id=%u context=%s\n",
      static_cast<unsigned>(mode), static_cast<unsigned>(connectionId), contextName());
  });
  keyboard.onOutputReport([](const EspBleHidKeyboardOutputReport &report) {
    Serial.printf("OUTPUT_REPORT leds=%u caps=%u context=%s\n",
      report.leds, report.capsLock() ? 1 : 0, contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Boot HID";
  config.security.enabled = false;
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("HID_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("HID_DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });

  ble.advertising().setName("EspBle Boot HID");
  ble.advertising().addServiceUuid("1812");
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
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u mode=%u\n",
        ble.advertising().isAdvertising() ? 1 : 0,
        static_cast<unsigned>(ble.hidKeyboard().protocolMode()));
    }
    else if (command == 'k')
    {
      EspBleHidKeyboardInputReport report;
      report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
      report.keys[0] = 0x04; // 'a'
      Serial.println(ble.hidKeyboard().sendReport(report) ? "INPUT_SENT" : "INPUT_SEND_FAILED");
    }
  }
  ble.update();
  delay(1);
}
