// en: ScanDump - diagnostic scanner that dumps every field EspBle extracts from each
//     advertisement (UUID form, name presence, manufacturer data, service data) and
//     decodes iBeacon payloads. Useful to see what a peripheral actually advertises
//     before writing a scan filter.
// ja: ScanDump - EspBleが各advertisementから取り出す全フィールド（UUID表記・nameの有無・
//     Manufacturer Data・Service Data）をダンプし、iBeacon payloadはデコードする診断用
//     スキャナ。scan filterを書く前に相手が実際に何をadvertiseしているか確認するのに使う。
#include <EspBle.h>
#include <EspBleIBeacon.h>

EspBle ble;

// en: Print a binary String as hex.
// ja: バイナリのStringをhexで表示する。
static void printHex(const String &data)
{
  for (size_t i = 0; i < data.length(); ++i)
  {
    Serial.printf("%02x", static_cast<uint8_t>(data[i]));
  }
}

// en: Print manufacturer data as hex.
// ja: Manufacturer Dataをhexで表示する。
static void printManufacturerData(const EspBleScanResult &scanResult)
{
  Serial.printf(" manufacturer[%u]=", static_cast<unsigned>(scanResult.manufacturerData.length()));
  printHex(scanResult.manufacturerData);
}

// en: Service Data is a payload tagged with the service UUID it belongs to. An
//     advertisement may carry several blocks, so print each one.
// ja: Service Dataは、どのserviceの値かをUUIDで示したpayload。1つのadvertisementに
//     複数ブロック載ることがあるので、すべて表示する。
static void printServiceData(const EspBleScanResult &scanResult)
{
  for (size_t i = 0; i < scanResult.serviceDataCount; ++i)
  {
    const EspBleServiceData &block = scanResult.serviceData[i];
    Serial.printf(
      " servicedata[%s][%u]=",
      block.uuid.c_str(),
      static_cast<unsigned>(block.data.length()));
    printHex(block.data);
  }
}

// en: iBeacon is a specific Manufacturer Data layout (Apple company ID 0x004C).
//     Decoding it here is what makes a beacon dump actually readable.
// ja: iBeaconはManufacturer Dataの特定レイアウト（Apple company ID 0x004C）。
//     ここでデコードすることでbeaconのダンプが読める形になる。
static void printIBeacon(const EspBleScanResult &scanResult)
{
  EspBleIBeaconData beacon;
  if (!espBleDecodeIBeacon(
        reinterpret_cast<const uint8_t *>(scanResult.manufacturerData.c_str()),
        scanResult.manufacturerData.length(),
        beacon))
  {
    return;
  }

  Serial.print(" ibeacon uuid=");
  for (size_t i = 0; i < sizeof(beacon.uuid); ++i)
  {
    Serial.printf("%02x", beacon.uuid[i]);
    // en: 8-4-4-4-12 grouping / ja: 8-4-4-4-12 の区切り
    if (i == 3 || i == 5 || i == 7 || i == 9) Serial.print('-');
  }
  Serial.printf(
    " major=%u minor=%u power=%d",
    static_cast<unsigned>(beacon.major),
    static_cast<unsigned>(beacon.minor),
    static_cast<int>(beacon.measuredPower));
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
    if (scanResult.hasAppearance())
    {
      // en: Device category the peer declares; hosts map it to an icon.
      // ja: 相手が申告する機器種別。ホスト側はアイコンに対応づける。
      Serial.printf(" appearance=0x%04x", scanResult.appearance);
    }
    if (scanResult.hasTxPowerLevel())
    {
      // en: Declared transmit power. rssi minus this is the path loss, which is
      //     the basis for any distance estimate.
      // ja: 申告された送信電力。rssiとの差が経路損失で、距離推定の基礎になる。
      Serial.printf(
        " txpower=%ddBm loss=%ddB",
        static_cast<int>(scanResult.txPowerLevel),
        static_cast<int>(scanResult.txPowerLevel) - scanResult.rssi);
    }
    for (size_t i = 0; i < scanResult.serviceUuidCount; ++i)
    {
      Serial.printf(" uuid=%s", scanResult.serviceUuids[i].c_str());
    }
    if (scanResult.hasServiceData())
    {
      printServiceData(scanResult);
    }
    if (scanResult.hasManufacturerData())
    {
      printManufacturerData(scanResult);
      printIBeacon(scanResult);
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
