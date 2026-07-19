// en: ScanDump - diagnostic scanner that dumps every field EspBle extracts from each
//     advertisement (UUID form, name presence, manufacturer data). Useful to see what a
//     peripheral actually advertises before writing a scan filter.
// ja: ScanDump - EspBleが各advertisementから取り出す全フィールド（UUID表記・nameの有無・
//     Manufacturer Data）をダンプする診断用スキャナ。scan filterを書く前に相手が実際に
//     何をadvertiseしているか確認するのに使う。
#include <EspBle.h>

EspBle ble;

// en: Print manufacturer data as hex.
// ja: Manufacturer Dataをhexで表示する。
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

  // en: Print all extracted fields for every advertisement.
  // ja: advertisementごとに取り出した全フィールドを表示する。
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    Serial.printf(
      "%s type=%u rssi=%d%s%s",
      scanResult.address.c_str(),
      static_cast<unsigned>(scanResult.addressType),
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
  scanConfig.active = true;       // en: also request scan responses (more names) / ja: scan responseも要求（nameが得やすい）
  scanConfig.durationSeconds = 0; // en: scan until reset / ja: リセットまでscan
  if (!ble.scanner().start(scanConfig))
  {
    Serial.printf("Scan failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  Serial.println("Scanning. Send 'q' to print diagnostic counters.");
}

void loop()
{
  // en: On 'q', print the drop counters (queue overflow diagnostics).
  // ja: 'q' で取りこぼしカウンタ（queue溢れの診断）を表示する。
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
