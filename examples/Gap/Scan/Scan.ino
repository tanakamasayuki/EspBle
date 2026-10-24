// en: Continuous active scan printing address / RSSI / name
// ja: address / RSSI / nameを表示する継続active scan
#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  // en: For scan-only, begin() without a config is fine.
  // ja: scanだけならconfigなしのbegin()でよい。
  if (!ble.begin())
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  // en: Scan results are copied to value types and delivered to this callback from
  //     the update() context (never on the BLE stack task).
  // ja: Scan Resultは値型としてcopyされ、update()のcontextでこのcallbackへ配送される
  //     （BLE stack task上では実行されない）。
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    Serial.printf("%s RSSI=%d", scanResult.address.c_str(), scanResult.rssi);
    // en: not every advertisement carries a name / ja: nameは全advertisementにあるとは限らない
    if (scanResult.hasName())
    {
      Serial.printf(" name=%s", scanResult.name.c_str());
    }
    Serial.println();
  });

  EspBleScanConfig scanConfig;
  // en: active scan (also requests scan responses)
  // ja: active scan（scan responseも要求）
  scanConfig.active = true;
  scanConfig.durationSeconds = 0;  // en: 0 = scan until reset / ja: 0 = リセットまで無制限
  if (!ble.scanner().start(scanConfig))
  {
    Serial.printf("Scan failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  // en: Scan results are not delivered unless update() is called.
  // ja: update() を呼ばないとScan Resultは配送されない。
  ble.update();
  delay(1);
}
