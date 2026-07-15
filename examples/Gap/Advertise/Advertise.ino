#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Advertiser";
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Advertiser");
  advertising.addServiceUuid("1812");
  if (!advertising.start())
  {
    Serial.printf("Advertising failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  ble.update();
  delay(1);
}
