// Central for the service_data peer test: scan and report every Service Data
// block, then look one up by UUID with serviceDataFor().
#include <EspBle.h>

static constexpr const char *LOOKUP_UUID = "181A";

EspBle ble;
bool reported = false;

static void printHex(const String &data)
{
  for (size_t i = 0; i < data.length(); ++i)
  {
    Serial.printf("%02x", static_cast<uint8_t>(data[i]));
  }
}

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

    Serial.printf("SERVICE_DATA_COUNT %u\n", static_cast<unsigned>(scanResult.serviceDataCount));
    for (size_t i = 0; i < scanResult.serviceDataCount; ++i)
    {
      const EspBleServiceData &block = scanResult.serviceData[i];
      Serial.printf(
        "SERVICE_DATA index=%u uuid=%s data=",
        static_cast<unsigned>(i),
        block.uuid.c_str());
      printHex(block.data);
      Serial.println();
    }

    // Look up by UUID: the 16-bit shorthand must match the 128-bit form the
    // scan result carries.
    String found;
    if (scanResult.serviceDataFor(LOOKUP_UUID, found))
    {
      Serial.print("SERVICE_DATA_LOOKUP data=");
      printHex(found);
      Serial.println();
    }
    else
    {
      Serial.println("SERVICE_DATA_LOOKUP missing");
    }
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
