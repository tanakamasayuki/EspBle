#include <EspBle.h>

EspBle ble;
uint8_t counter = 0;

static void printReport(const char *label, const EspBleHidVendorReport &report)
{
  Serial.printf("%s type=%u length=%u data=", label, report.reportType,
    static_cast<unsigned>(report.length));
  for (size_t index = 0; index < report.length; ++index)
    Serial.printf("%s%02x", index == 0 ? "" : " ", report.data[index]);
  Serial.println();
}

void setup()
{
  Serial.begin(115200);

  EspBleHidVendorConfig vendorConfig;
  vendorConfig.reportSize = 8;
  if (!ble.hidVendor().configure(vendorConfig))
  {
    Serial.printf("Vendor HID configuration failed: %s\n",
      ble.lastErrorDetail().c_str());
    return;
  }
  ble.hidVendor().onOutputReport([](const EspBleHidVendorReport &report) {
    printReport("Output", report);
  });
  ble.hidVendor().onFeatureReport([](const EspBleHidVendorReport &report) {
    printReport("Feature", report);
  });

  EspBleConfig config;
  config.deviceName = "EspBle Vendor HID";
  config.preferredMtu = 100;
  config.security.enabled = true;
  config.security.bonding = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });
  ble.advertising().setName(config.deviceName);
  ble.advertising().start();
  Serial.println("Send 'i' to send an 8-byte Vendor Input Report.");
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == 'i')
  {
    const uint8_t report[] = {'E', 'S', 'P', counter++, 4, 5, 6, 7};
    Serial.printf("Input: %s\n",
      ble.hidVendor().sendInput(report, sizeof(report)) ? "sent" : "failed");
  }
  ble.update();
  delay(1);
}
