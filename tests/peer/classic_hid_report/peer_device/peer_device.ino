#include <EspBleClassic.h>

EspBleClassic bluetooth;
EspBleClassicSppSessionId sppSession = 0;

void printHex(const String &value)
{
  for (size_t index = 0; index < value.length(); ++index)
    Serial.printf("%02x", static_cast<uint8_t>(value[index]));
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  bluetooth.hidHost().onConnected([](const EspBleClassicHidConnection &event) {
    Serial.printf("CLASSIC_HIDH_CONNECTED peer=%s\n", event.peerAddress.c_str());
  });
  bluetooth.hidHost().onDisconnected([](const EspBleClassicHidConnection &) {
    Serial.println("CLASSIC_HIDH_DISCONNECTED");
  });
  bluetooth.hidHost().onInputReport([](const EspBleClassicHidReport &event) {
    Serial.printf(
      "CLASSIC_HIDH_INPUT id=%u length=%u hex=",
      event.reportId, static_cast<unsigned>(event.value.length()));
    printHex(event.value);
    Serial.println();
    const uint8_t output[] = {0x02, 0xa5, 0x00, 0xff};
    Serial.printf(
      "CLASSIC_HIDH_OUTPUT_ACCEPTED %u\n",
      bluetooth.hidHost().sendOutputReport(output, sizeof(output)) ? 1 : 0);
  });
  bluetooth.spp().onConnected([](const EspBleClassicSppSession &session) {
    sppSession = session.id;
    Serial.printf("CLASSIC_COMPOSED_SPP_CONNECTED id=%u\n", session.id);
    const uint8_t payload[] = {0x00, 0x53, 0x50, 0x50, 0xff};
    Serial.printf(
      "CLASSIC_COMPOSED_SPP_WRITE %u\n",
      bluetooth.spp().write(session.id, payload, sizeof(payload)) ? 1 : 0);
  });
  bluetooth.spp().onData([](const EspBleClassicSppData &event) {
    Serial.printf("CLASSIC_COMPOSED_SPP_ECHO hex=");
    printHex(event.value);
    Serial.println();
  });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic HID Host";
  if (!bluetooth.begin(config) || !bluetooth.hidHost().begin())
  {
    Serial.printf("CLASSIC_HIDH_BEGIN_FAILED %s\n", bluetooth.lastErrorDetail().c_str());
    return;
  }
  Serial.println("CLASSIC_HIDH_READY");
}

void loop()
{
  bluetooth.update();
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.startsWith("c"))
    {
      Serial.printf(
        "CLASSIC_HIDH_CONNECT_ACCEPTED %u\n",
        bluetooth.hidHost().connect(command.c_str() + 1) ? 1 : 0);
    }
    else if (command == "d")
    {
      Serial.printf(
        "CLASSIC_HIDH_DISCONNECT_ACCEPTED %u\n",
        bluetooth.hidHost().disconnect() ? 1 : 0);
    }
    else if (command.startsWith("s"))
    {
      Serial.printf(
        "CLASSIC_COMPOSED_SPP_CONNECT %u\n",
        bluetooth.spp().connect(command.c_str() + 1) ? 1 : 0);
    }
  }
  delay(1);
}
