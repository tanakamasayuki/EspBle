// Central for the service_data peer test: scan and read a Service Data block.
#include <EspBle.h>

EspBle ble;
bool reported = false;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Service Data Observer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (reported || !scanResult.hasServiceData()) return;
    reported = true;
    ble.scanner().stop();
    const String &data = scanResult.serviceData;
    char hex[2 * 8 + 1] = {0};
    const size_t count = data.length() < 8 ? data.length() : 8;
    for (size_t i = 0; i < count; ++i)
      snprintf(hex + i * 2, 3, "%02x", static_cast<uint8_t>(data[i]));
    Serial.printf("SERVICE_DATA uuid=%s data=%s\n",
      scanResult.serviceDataUuid.c_str(), hex);
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
