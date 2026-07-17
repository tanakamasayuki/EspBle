#include <EspBle.h>

// Diagnostic scanner: dumps every field EspBle extracts from each
// advertisement. Useful to check what a peripheral actually advertises
// (UUID form, name presence, manufacturer data) before writing a filter.

EspBle ble;

static void printManufacturerData(const EspBleScanResult &scanResult)
{
  Serial.printf(" manufacturer[%u]=", static_cast<unsigned>(scanResult.manufacturerData.length()));
  for (size_t i = 0; i < scanResult.manufacturerData.length(); ++i)
  {
    Serial.printf("%02x", static_cast<uint8_t>(scanResult.manufacturerData[i]));
  }
}

void setup()
{
  Serial.begin(115200);

  if (!ble.begin())
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    Serial.printf(
      "%s type=%u rssi=%d%s%s",
      scanResult.address.c_str(),
      scanResult.addressType,
      scanResult.rssi,
      scanResult.connectable ? " connectable" : "",
      scanResult.scannable ? " scannable" : "");
    if (scanResult.hasName())
    {
      Serial.printf(" name=\"%s\"", scanResult.name.c_str());
    }
    for (size_t i = 0; i < scanResult.serviceUuidCount; ++i)
    {
      Serial.printf(" uuid=%s", scanResult.serviceUuids[i].c_str());
    }
    if (scanResult.hasManufacturerData())
    {
      printManufacturerData(scanResult);
    }
    Serial.println();
  });

  EspBleScanConfig scanConfig;
  scanConfig.active = true;       // also request scan responses (more names)
  scanConfig.durationSeconds = 0; // scan until reset
  if (!ble.scanner().start(scanConfig))
  {
    Serial.printf("Scan failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  Serial.println("Scanning. Send 'q' to print diagnostic counters.");
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == 'q')
  {
    Serial.printf(
      "counters: droppedScanResults=%u droppedEvents=%u\n",
      static_cast<unsigned>(ble.scanner().droppedResultCount()),
      static_cast<unsigned>(ble.droppedEventCount()));
  }

  ble.update();
  delay(1);
}
