// Central for the ibeacon peer test: scan and decode an iBeacon advertisement.
#include <EspBle.h>
#include <EspBleIBeacon.h>

EspBle ble;
bool reported = false;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle iBeacon Observer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (reported || !scanResult.hasManufacturerData()) return;
    const String &manufacturer = scanResult.manufacturerData;
    EspBleIBeaconData beacon;
    if (!espBleDecodeIBeacon(
          reinterpret_cast<const uint8_t *>(manufacturer.c_str()),
          manufacturer.length(), beacon))
    {
      return;
    }
    reported = true;
    ble.scanner().stop();
    char uuidHex[33];
    for (size_t i = 0; i < 16; ++i)
      snprintf(uuidHex + i * 2, 3, "%02x", beacon.uuid[i]);
    Serial.printf(
      "IBEACON uuid=%s major=%u minor=%u power=%d connectable=%u scannable=%u\n",
      uuidHex,
      static_cast<unsigned>(beacon.major),
      static_cast<unsigned>(beacon.minor),
      static_cast<int>(beacon.measuredPower),
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
