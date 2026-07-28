// Central for the scan_response peer test: scan the advertiser passively and
// then actively, reporting which fields arrive in each mode. The scan response
// fields (name, manufacturer data) must only appear in the active scan.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "FEAC";

EspBle ble;
bool reported = false;
const char *mode = "";

static void reportResult(const EspBleScanResult &scanResult)
{
  char hex[2 * 8 + 1] = {0};
  const String &data = scanResult.manufacturerData;
  const size_t count = data.length() < 8 ? data.length() : 8;
  for (size_t i = 0; i < count; ++i)
  {
    snprintf(hex + i * 2, 3, "%02x", static_cast<uint8_t>(data[i]));
  }
  Serial.printf(
    "RESULT mode=%s name=\"%s\" manufacturer=%s\n",
    mode,
    scanResult.name.c_str(),
    data.isEmpty() ? "-" : hex);
}

static void startScan(bool active, const char *label)
{
  reported = false;
  mode = label;
  EspBleScanConfig scanConfig;
  scanConfig.active = active;
  Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Scan Response Observer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (reported || !scanResult.advertisesService(SERVICE_UUID)) return;
    reported = true;
    ble.scanner().stop();
    reportResult(scanResult);
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'p')
    {
      startScan(false, "passive");
    }
    else if (command == 'a')
    {
      startScan(true, "active");
    }
  }

  ble.update();
  delay(1);
}
