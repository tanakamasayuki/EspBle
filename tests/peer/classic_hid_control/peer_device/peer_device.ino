// The HID Host side of the control-channel exchanges. Every call here is a
// round trip: the request is accepted locally and the answer arrives as an
// event, which is why each one has its own result callback.
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void printHex(const String &value)
{
  for (size_t index = 0; index < value.length(); ++index)
    Serial.printf("%02x", static_cast<uint8_t>(value[index]));
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  bluetooth.hidHost().onConnected(
    [](const EspBleClassicHidConnection &connection) {
      Serial.printf("CONTROL_HOST_CONNECTED peer=%s\n",
        connection.peerAddress.c_str());
    });
  bluetooth.hidHost().onDisconnected(
    [](const EspBleClassicHidConnection &) {
      Serial.println("CONTROL_HOST_DISCONNECTED");
    });
  bluetooth.hidHost().onReportResult(
    [](const EspBleClassicHidHost::ReportResult &result) {
      Serial.printf("CONTROL_HOST_REPORT success=%u hex=",
        result.success ? 1 : 0);
      printHex(result.value);
      Serial.println();
    });
  bluetooth.hidHost().onReportSent(
    [](const EspBleClassicHidHost::ReportResult &result) {
      Serial.printf("CONTROL_HOST_SENT success=%u\n", result.success ? 1 : 0);
    });
  bluetooth.hidHost().onProtocolMode(
    [](const EspBleClassicHidHost::ProtocolModeResult &result) {
      Serial.printf("CONTROL_HOST_PROTOCOL success=%u mode=%u\n",
        result.success ? 1 : 0, static_cast<unsigned>(result.mode));
    });
  bluetooth.hidHost().onIdleRate(
    [](const EspBleClassicHidHost::IdleRateResult &result) {
      Serial.printf("CONTROL_HOST_IDLE success=%u rate=%u\n",
        result.success ? 1 : 0, result.idleRate);
    });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic HID Control Host";
  if (!bluetooth.begin(config) || !bluetooth.hidHost().begin())
  {
    Serial.printf("CONTROL_HOST_FAILED error=%s\n", bluetooth.lastErrorName());
    return;
  }
  Serial.println("CONTROL_HOST_READY");
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    const String line = Serial.readStringUntil('\n');
    if (line.length() == 0) return;
    const char command = line[0];
    if (command == 'c')
      Serial.printf("CONTROL_HOST_CONNECT requested=%u\n",
        bluetooth.hidHost().connect(line.substring(1).c_str()) ? 1 : 0);
    else if (command == 'g')
      Serial.printf("CONTROL_HOST_GET requested=%u\n",
        bluetooth.hidHost().requestReport(
          EspBleClassicHidReportType::Input, 1, 8) ? 1 : 0);
    else if (command == 'G')
      // A report ID the descriptor does not declare, so the device answers with
      // a refusal rather than data.
      Serial.printf("CONTROL_HOST_GET requested=%u\n",
        bluetooth.hidHost().requestReport(
          EspBleClassicHidReportType::Input, 9, 8) ? 1 : 0);
    else if (command == 's')
    {
      // A Feature report: the type that only travels on the control channel.
      // The report ID leads the payload, as with every raw report here.
      const uint8_t feature[] = {0x03, 0xab, 0xcd};
      Serial.printf("CONTROL_HOST_SET requested=%u\n",
        bluetooth.hidHost().sendReport(
          EspBleClassicHidReportType::Feature, feature,
          sizeof(feature)) ? 1 : 0);
    }
    else if (command == 'b')
      Serial.printf("CONTROL_HOST_SET_PROTOCOL requested=%u\n",
        bluetooth.hidHost().setProtocolMode(
          EspBleClassicHidProtocolMode::Boot) ? 1 : 0);
    else if (command == 'B')
      Serial.printf("CONTROL_HOST_SET_PROTOCOL requested=%u\n",
        bluetooth.hidHost().setProtocolMode(
          EspBleClassicHidProtocolMode::Report) ? 1 : 0);
    else if (command == 'p')
      Serial.printf("CONTROL_HOST_GET_PROTOCOL requested=%u\n",
        bluetooth.hidHost().requestProtocolMode() ? 1 : 0);
    else if (command == 'i')
      Serial.printf("CONTROL_HOST_SET_IDLE requested=%u\n",
        bluetooth.hidHost().setIdleRate(0) ? 1 : 0);
    else if (command == 'I')
      Serial.printf("CONTROL_HOST_GET_IDLE requested=%u\n",
        bluetooth.hidHost().requestIdleRate() ? 1 : 0);
    else if (command == 'u')
      Serial.printf("CONTROL_HOST_UNPLUG requested=%u\n",
        bluetooth.hidHost().virtualCableUnplug() ? 1 : 0);
    else if (command == '?')
      Serial.printf("CONTROL_HOST_STATE connected=%u error=%s\n",
        bluetooth.hidHost().connected() ? 1 : 0,
        bluetooth.lastErrorName());
  }
  delay(1);
}
