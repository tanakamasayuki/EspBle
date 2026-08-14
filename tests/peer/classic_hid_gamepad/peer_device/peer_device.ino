// The Host side. A gamepad report is not decoded into events by this library, so
// it arrives raw — which is exactly what makes the byte layout checkable.
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);
  delay(500);

  bluetooth.hidHost().onConnected(
    [](const EspBleClassicHidConnection &connection) {
      Serial.printf("GAMEPAD_HOST_CONNECTED peer=%s\n",
        connection.peerAddress.c_str());
    });
  bluetooth.hidHost().onInputReport(
    [](const EspBleClassicHidReport &report) {
      Serial.printf("GAMEPAD_HOST_RAW id=%u len=%u hex=", report.reportId,
        static_cast<unsigned>(report.value.length()));
      for (size_t index = 0; index < report.value.length(); ++index)
        Serial.printf("%02x", static_cast<uint8_t>(report.value[index]));
      Serial.println();
    });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Gamepad Host";
  if (!bluetooth.begin(config) || !bluetooth.hidHost().begin())
  {
    Serial.printf("GAMEPAD_HOST_FAILED error=%s\n", bluetooth.lastErrorName());
    return;
  }
  Serial.println("GAMEPAD_HOST_READY");
}

void loop()
{
  bluetooth.update();

  if (Serial.available())
  {
    const String line = Serial.readStringUntil('\n');
    if (line.length() == 0) return;
    if (line[0] == 'c')
      Serial.printf("GAMEPAD_HOST_CONNECT requested=%u\n",
        bluetooth.hidHost().connect(line.substring(1).c_str()) ? 1 : 0);
  }
  delay(1);
}
