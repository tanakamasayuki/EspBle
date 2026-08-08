#include <EspBleClassic.h>

static const uint8_t ReportDescriptor[] = {
  0x06, 0x00, 0xff, // Usage Page (Vendor 0xff00)
  0x09, 0x01,       // Usage 1
  0xa1, 0x01,       // Collection (Application)
  0x85, 0x01,       // Report ID 1
  0x15, 0x00,       // Logical Minimum 0
  0x26, 0xff, 0x00, // Logical Maximum 255
  0x75, 0x08,       // Report Size 8
  0x95, 0x04,       // Report Count 4
  0x09, 0x01,       // Usage 1
  0x81, 0x02,       // Input (Data, Variable, Absolute)
  0xc0,             // End Collection
};

EspBleClassic bluetooth;
uint8_t counter = 0;
uint32_t nextReport = 0;

void setup()
{
  Serial.begin(115200);

  bluetooth.hidDevice().onConnected([](const EspBleClassicHidConnection &event) {
    Serial.printf("HID host connected: %s\n", event.peerAddress.c_str());
  });
  bluetooth.hidDevice().onDisconnected([](const EspBleClassicHidConnection &) {
    Serial.println("HID host disconnected");
  });

  EspBleClassicConfig stackConfig;
  stackConfig.deviceName = "EspBle Classic Vendor HID";
  if (!bluetooth.begin(stackConfig))
  {
    Serial.println(bluetooth.lastErrorDetail());
    return;
  }

  EspBleClassicHidDeviceConfig hidConfig;
  hidConfig.name = "EspBle Vendor HID";
  hidConfig.reportDescriptor = ReportDescriptor;
  hidConfig.reportDescriptorLength = sizeof(ReportDescriptor);
  if (!bluetooth.hidDevice().begin(hidConfig))
    Serial.println(bluetooth.lastErrorDetail());
}

void loop()
{
  bluetooth.update();
  if (
    bluetooth.hidDevice().connected() &&
    static_cast<int32_t>(millis() - nextReport) >= 0)
  {
    const uint8_t report[] = {counter++, 0x7f, 0x80, 0xff};
    bluetooth.hidDevice().sendInputReport(1, report, sizeof(report));
    nextReport = millis() + 1000;
  }
  delay(1);
}
