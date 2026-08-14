// The HID Device side of the control-channel exchanges: Get_Report, Set_Report
// and the protocol mode. A real Host asks for these after connecting, and a
// device that never answers looks broken to it.
#include <EspBleClassic.h>
#include <esp_mac.h>

// Report 1 is an Input report the Host can ask for, report 2 an Output report,
// and report 3 a Feature report — the type that only exists on the control
// channel, so it is the one that proves Set_Report keeps its type.
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
  0x85, 0x03,             // Report ID 3
  0x95, 0x02,             // Report Count 2
  0x09, 0x03,             // Usage 3
  0xb1, 0x02,             // Feature (Data, Variable, Absolute)
  0xc0,                   // End Collection
};

EspBleClassic bluetooth;
// The value the Host reads back. Report 1 is four bytes wide.
uint8_t inputReport[] = {0x10, 0x20, 0x30, 0x40};
bool refuseRequests = false;

void printHex(const String &value)
{
  for (size_t index = 0; index < value.length(); ++index)
    Serial.printf("%02x", static_cast<uint8_t>(value[index]));
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  bluetooth.hidDevice().onConnected(
    [](const EspBleClassicHidConnection &connection) {
      Serial.printf("CONTROL_DEVICE_CONNECTED peer=%s\n",
        connection.peerAddress.c_str());
    });
  bluetooth.hidDevice().onDisconnected(
    [](const EspBleClassicHidConnection &) {
      Serial.println("CONTROL_DEVICE_DISCONNECTED");
    });

  bluetooth.hidDevice().onReportRequested(
    [](const EspBleClassicHidReportRequest &request) {
      Serial.printf("CONTROL_DEVICE_GET type=%u id=%u max=%u\n",
        static_cast<unsigned>(request.type), request.reportId,
        request.maximumLength);
      if (refuseRequests)
      {
        // Refusing is an answer too: the Host stops waiting instead of hitting
        // its own timeout, which some Hosts treat as a dead device.
        Serial.printf("CONTROL_DEVICE_REFUSED sent=%u\n",
          bluetooth.hidDevice().refuseReportRequest(
            EspBleClassicHidRequestError::InvalidReportId) ? 1 : 0);
        return;
      }
      // The request carries the type and report ID the Host used, so passing it
      // back is what keeps the answer matched to the question.
      Serial.printf("CONTROL_DEVICE_ANSWERED sent=%u\n",
        bluetooth.hidDevice().respondToReportRequest(
          request, inputReport, sizeof(inputReport)) ? 1 : 0);
    });

  bluetooth.hidDevice().onSetReport(
    [](const EspBleClassicHidReport &report) {
      Serial.printf("CONTROL_DEVICE_SET type=%u id=%u hex=",
        static_cast<unsigned>(report.type), report.reportId);
      printHex(report.value);
      Serial.println();
    });

  bluetooth.hidDevice().onProtocolMode(
    [](EspBleClassicHidProtocolMode mode) {
      Serial.printf("CONTROL_DEVICE_PROTOCOL mode=%u\n",
        static_cast<unsigned>(mode));
    });

  EspBleClassicConfig stackConfig;
  stackConfig.deviceName = "EspBle Classic HID Control";
  EspBleClassicHidDeviceConfig hidConfig;
  hidConfig.name = "EspBle Control HID";
  hidConfig.description = "EspBle Classic HID control test";
  hidConfig.provider = "EspBle";
  hidConfig.reportDescriptor = ReportDescriptor;
  hidConfig.reportDescriptorLength = sizeof(ReportDescriptor);
  if (!bluetooth.begin(stackConfig) ||
      !bluetooth.hidDevice().begin(hidConfig))
  {
    Serial.printf("CONTROL_DEVICE_FAILED error=%s detail=%s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  Serial.printf(
    "CONTROL_DEVICE_READY address=%02x:%02x:%02x:%02x:%02x:%02x\n",
    address[0], address[1], address[2], address[3], address[4], address[5]);
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    const char command = Serial.read();
    if (command == 'r')
    {
      refuseRequests = true;
      Serial.println("CONTROL_DEVICE_MODE refuse");
    }
    else if (command == 'a')
    {
      refuseRequests = false;
      Serial.println("CONTROL_DEVICE_MODE answer");
    }
    else if (command == 'w')
    {
      // Answering with nothing pending would put an unsolicited report on the
      // control channel, so it has to be refused locally.
      EspBleClassicHidReportRequest stale;
      stale.type = EspBleClassicHidReportType::Input;
      stale.reportId = 1;
      stale.maximumLength = sizeof(inputReport);
      Serial.printf("CONTROL_DEVICE_STALE sent=%u error=%s\n",
        bluetooth.hidDevice().respondToReportRequest(
          stale, inputReport, sizeof(inputReport)) ? 1 : 0,
        bluetooth.lastErrorName());
    }
    else if (command == '?')
      Serial.printf("CONTROL_DEVICE_STATE connected=%u protocol=%u dropped=%u\n",
        bluetooth.hidDevice().connected() ? 1 : 0,
        static_cast<unsigned>(bluetooth.hidDevice().protocolMode()),
        static_cast<unsigned>(bluetooth.hidDevice().droppedEventCount()));
  }
  delay(1);
}
