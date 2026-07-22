// Central for the eddystone peer test: scan and decode Eddystone URL / UID / TLM
// frames from the Service Data.
#include <EspBle.h>
#include <EspBleEddystone.h>

EspBle ble;
bool reported = false;

static void report(const uint8_t *data, size_t length, const EspBleScanResult &scanResult)
{
  char url[64];
  int8_t txPower = 0;
  EspBleEddystoneUidData uid;
  EspBleEddystoneTlmData tlm;

  if (espBleDecodeEddystoneUrl(data, length, url, sizeof(url), txPower))
  {
    Serial.printf("EDDYSTONE_URL url=%s tx=%d connectable=%u scannable=%u\n",
      url, static_cast<int>(txPower),
      scanResult.connectable ? 1 : 0, scanResult.scannable ? 1 : 0);
  }
  else if (espBleDecodeEddystoneUid(data, length, uid))
  {
    char ns[21];
    char inst[13];
    for (int i = 0; i < 10; ++i) snprintf(ns + i * 2, 3, "%02x", uid.namespaceId[i]);
    for (int i = 0; i < 6; ++i) snprintf(inst + i * 2, 3, "%02x", uid.instanceId[i]);
    Serial.printf("EDDYSTONE_UID namespace=%s instance=%s tx=%d\n",
      ns, inst, static_cast<int>(uid.txPower));
  }
  else if (espBleDecodeEddystoneTlm(data, length, tlm))
  {
    Serial.printf("EDDYSTONE_TLM battery=%u temp=%d count=%lu uptime=%lu\n",
      static_cast<unsigned>(tlm.batteryMilliVolts),
      static_cast<int>(tlm.temperatureCelsius * 10.0f + (tlm.temperatureCelsius >= 0 ? 0.5f : -0.5f)),
      static_cast<unsigned long>(tlm.advertisingCount),
      static_cast<unsigned long>(tlm.uptimeDeciseconds));
  }
  else
  {
    return;
  }
  reported = true;
  ble.scanner().stop();
}

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
    report(reinterpret_cast<const uint8_t *>(scanResult.serviceData.c_str()),
           scanResult.serviceData.length(), scanResult);
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
      scanConfig.wantDuplicates = true; // report every advertisement, not just the first
      Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
  }

  ble.update();
  delay(1);
}
