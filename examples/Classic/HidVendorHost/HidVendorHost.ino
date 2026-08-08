#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  bluetooth.hidHost().onConnected([](const EspBleClassicHidConnection &event) {
    Serial.printf("HID device connected: %s\n", event.peerAddress.c_str());
  });
  bluetooth.hidHost().onInputReport([](const EspBleClassicHidReport &event) {
    Serial.printf("Input report ID %u:", event.reportId);
    for (size_t index = 0; index < event.value.length(); ++index)
      Serial.printf(" %02x", static_cast<uint8_t>(event.value[index]));
    Serial.println();
  });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic HID Host";
  if (!bluetooth.begin(config) || !bluetooth.hidHost().begin())
    Serial.println(bluetooth.lastErrorDetail());
  else
    Serial.println("Enter the HID device address, for example aa:bb:cc:dd:ee:ff");
}

void loop()
{
  bluetooth.update();
  if (Serial.available())
  {
    String address = Serial.readStringUntil('\n');
    address.trim();
    if (!bluetooth.hidHost().connect(address.c_str()))
      Serial.println(bluetooth.lastErrorDetail());
  }
  delay(1);
}
