// Central for the address_privacy test: scan for the peer and report the
// address type / value it advertises with.
#include <EspBle.h>

static constexpr const char *MARKER_SERVICE_UUID = "70726976-6163-7900-9003-72616e646d01";

EspBle ble;
bool reported = false;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Privacy Observer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (reported || !scanResult.advertisesService(MARKER_SERVICE_UUID)) return;
    reported = true;
    ble.scanner().stop();
    Serial.printf("PEER_SEEN addr=%s type=%u\n",
      scanResult.address.c_str(), static_cast<unsigned>(scanResult.addressType));
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
