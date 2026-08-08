#include <EspBleClassic.h>
#include <esp_mac.h>

static const uint8_t ReportDescriptor[] = {
  0x06, 0x00, 0xff,       // Usage Page (Vendor 0xff00)
  0x09, 0x01,             // Usage 1
  0xa1, 0x01,             // Collection (Application)
  0x85, 0x01,             // Report ID 1
  0x15, 0x00,             // Logical Minimum 0
  0x26, 0xff, 0x00,       // Logical Maximum 255
  0x75, 0x08,             // Report Size 8
  0x95, 0x04,             // Report Count 4
  0x09, 0x01,             // Usage 1
  0x81, 0x02,             // Input (Data, Variable, Absolute)
  0x85, 0x02,             // Report ID 2
  0x95, 0x03,             // Report Count 3
  0x09, 0x02,             // Usage 2
  0x91, 0x02,             // Output (Data, Variable, Absolute)
  0xc0,                   // End Collection
};

EspBleClassic bluetooth;
bool inputSent = false;

void printHex(const String &value)
{
  for (size_t index = 0; index < value.length(); ++index)
    Serial.printf("%02x", static_cast<uint8_t>(value[index]));
}

bool startDevice()
{
  inputSent = false;
  EspBleClassicConfig stackConfig;
  stackConfig.deviceName = "EspBle Classic HID Device";
  if (!bluetooth.begin(stackConfig))
  {
    Serial.printf("CLASSIC_HIDD_STACK_FAILED %s\n", bluetooth.lastErrorDetail().c_str());
    return false;
  }
  EspBleClassicHidDeviceConfig hidConfig;
  hidConfig.name = "EspBle Vendor HID";
  hidConfig.description = "EspBle Classic HID report test";
  hidConfig.provider = "EspBle";
  hidConfig.subclass = 0;
  hidConfig.reportDescriptor = ReportDescriptor;
  hidConfig.reportDescriptorLength = sizeof(ReportDescriptor);
  if (!bluetooth.hidDevice().begin(hidConfig))
  {
    Serial.printf("CLASSIC_HIDD_BEGIN_FAILED %s\n", bluetooth.lastErrorDetail().c_str());
    return false;
  }
  if (!bluetooth.spp().startServer())
  {
    Serial.printf("CLASSIC_HIDD_SPP_FAILED %s\n", bluetooth.lastErrorDetail().c_str());
    return false;
  }
  uint8_t address[6] = {};
  if (esp_read_mac(address, ESP_MAC_BT) != ESP_OK)
  {
    Serial.println("CLASSIC_HIDD_ADDRESS_FAILED");
    return false;
  }
  Serial.printf(
    "CLASSIC_HIDD_READY address=%02x:%02x:%02x:%02x:%02x:%02x\n",
    address[0], address[1], address[2], address[3], address[4], address[5]);
  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  bluetooth.hidDevice().onConnected([](const EspBleClassicHidConnection &event) {
    Serial.printf("CLASSIC_HIDD_CONNECTED peer=%s\n", event.peerAddress.c_str());
  });
  bluetooth.hidDevice().onDisconnected([](const EspBleClassicHidConnection &) {
    Serial.println("CLASSIC_HIDD_DISCONNECTED");
  });
  bluetooth.hidDevice().onOutputReport([](const EspBleClassicHidReport &event) {
    Serial.printf(
      "CLASSIC_HIDD_OUTPUT id=%u length=%u hex=",
      event.reportId, static_cast<unsigned>(event.value.length()));
    printHex(event.value);
    Serial.println();
  });
  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    Serial.printf("CLASSIC_COMPOSED_SPP_CONNECTED id=%u\n", session.id);
  });
  bluetooth.spp().onData([](const EspBleClassicSppData &event) {
    Serial.printf("CLASSIC_COMPOSED_SPP_RX length=%u\n", event.value.length());
    bluetooth.spp().write(event.sessionId, event.value);
  });

  startDevice();
}

void loop()
{
  bluetooth.update();
  if (bluetooth.hidDevice().connected() && !inputSent)
  {
    const uint8_t input[] = {0x00, 0x7f, 0x80, 0xff};
    inputSent = bluetooth.hidDevice().sendInputReport(1, input, sizeof(input));
    Serial.printf("CLASSIC_HIDD_INPUT_ACCEPTED %u\n", inputSent ? 1 : 0);
  }
  if (Serial.available())
  {
    const char command = static_cast<char>(Serial.read());
    if (command == 'r')
    {
      bluetooth.end();
      Serial.println("CLASSIC_HIDD_ENDED");
      delay(100);
      startDevice();
    }
    else if (command == 'i')
    {
      inputSent = false;
    }
  }
  delay(1);
}
