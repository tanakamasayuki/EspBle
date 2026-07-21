// hid_custom peer_device: a Custom HID device built with ble.hidCustom() using an
// arbitrary (vendor-defined) Report Descriptor composed into the HID service. It
// declares a 2-byte input report (dial delta + buttons) and a 1-byte output
// report (LED), both under Report ID 1. Security is disabled so a generic GATT
// client can read the Report Map and subscribe to the input report directly.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Vendor-defined usage page 0xFF00: Report ID 1 with a 2-byte input (signed dial
// delta + button bitfield) and a 1-byte output (LED state).
static const uint8_t customReportMap[] = {
  0x06, 0x00, 0xFF,   // Usage Page (Vendor-Defined 0xFF00)
  0x09, 0x01,         // Usage (0x01)
  0xA1, 0x01,         // Collection (Application)
  0x85, 0x01,         //   Report ID (1)
  0x15, 0x81,         //   Logical Minimum (-127)
  0x25, 0x7F,         //   Logical Maximum (127)
  0x75, 0x08,         //   Report Size (8)
  0x95, 0x02,         //   Report Count (2)
  0x09, 0x02,         //   Usage (0x02)
  0x81, 0x02,         //   Input (Data, Variable, Absolute)
  0x15, 0x00,         //   Logical Minimum (0)
  0x25, 0x01,         //   Logical Maximum (1)
  0x75, 0x08,         //   Report Size (8)
  0x95, 0x01,         //   Report Count (1)
  0x09, 0x03,         //   Usage (0x03)
  0x91, 0x02,         //   Output (Data, Variable, Absolute)
  0xC0,               // End Collection
};

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

  auto &custom = ble.hidCustom();
  EspBleHidDeviceConfig config;
  config.manufacturer = "EspBle Test";
  config.vendorId = 0x303a;
  config.productId = 0x4003;
  // Input registered first so a UUID-addressed generic client reaches it.
  if (!custom.configure(config) ||
      !custom.addInputReport(1, 2) ||
      !custom.addOutputReport(1, 1) ||
      !custom.setReportMap(customReportMap, sizeof(customReportMap)))
  {
    Serial.printf("HID_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  custom.onOutputReport([](const EspBleHidVendorReport &report) {
    Serial.printf("OUTPUT_REPORT id=%u len=%u byte0=%u context=%s\n",
      static_cast<unsigned>(report.reportId),
      static_cast<unsigned>(report.length),
      report.length > 0 ? report.data[0] : 0,
      contextName());
  });

  EspBleConfig bleConfig;
  bleConfig.deviceName = "EspBle Custom HID";
  bleConfig.security.enabled = false;
  if (!ble.begin(bleConfig))
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

  ble.advertising().setName("EspBle Custom HID");
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
      Serial.printf("ADVERTISING %u maplen=%u\n",
        ble.advertising().isAdvertising() ? 1 : 0,
        static_cast<unsigned>(sizeof(customReportMap)));
    }
    else if (command == 'i')
    {
      // dial delta = +5, buttons = 0x01
      const uint8_t report[2] = {static_cast<uint8_t>(5), 0x01};
      Serial.println(ble.hidCustom().sendInput(1, report, sizeof(report))
        ? "INPUT_SENT" : "INPUT_SEND_FAILED");
    }
  }
  ble.update();
  delay(1);
}
