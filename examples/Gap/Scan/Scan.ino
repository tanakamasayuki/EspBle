#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  if (!ble.begin())
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    Serial.printf("%s RSSI=%d", scanResult.address.c_str(), scanResult.rssi);
    if (scanResult.hasName())
    {
      Serial.printf(" name=%s", scanResult.name.c_str());
    }
    Serial.println();
  });

  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  scanConfig.durationSeconds = 0;
  if (!ble.scanner().start(scanConfig))
  {
    Serial.printf("Scan failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  ble.update();
  delay(1);
}
