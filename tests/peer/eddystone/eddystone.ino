// Central for the eddystone peer test: scan and decode an Eddystone-URL frame.
#include <EspBle.h>
#include <EspBleEddystone.h>

EspBle ble;
bool reported = false;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Eddystone Observer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (reported || !scanResult.hasServiceData()) return;
    const String &serviceData = scanResult.serviceData;
    char url[64];
    int8_t txPower = 0;
    if (!espBleDecodeEddystoneUrl(
          reinterpret_cast<const uint8_t *>(serviceData.c_str()),
          serviceData.length(), url, sizeof(url), txPower))
    {
      return;
    }
    reported = true;
    ble.scanner().stop();
    Serial.printf(
      "EDDYSTONE url=%s tx=%d connectable=%u scannable=%u\n",
      url,
      static_cast<int>(txPower),
      scanResult.connectable ? 1 : 0,
      scanResult.scannable ? 1 : 0);
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's')
    {
      reported = false;
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
  }

  ble.update();
  delay(1);
}
