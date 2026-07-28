// Central for the local_identity peer test: scan the peripheral to observe the
// address and Tx Power it actually transmits, connect, and report the reason
// code the peripheral used when it disconnected.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "FEAE";

EspBle ble;
bool reported = false;
bool connectRequested = false;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Local Identity Central";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (reported || !scanResult.advertisesService(SERVICE_UUID)) return;
    reported = true;
    ble.scanner().stop();
    Serial.printf(
      "OBSERVED address=%s type=%u txpower=%s\n",
      scanResult.address.c_str(),
      static_cast<unsigned>(scanResult.addressType),
      scanResult.hasTxPowerLevel() ? String(scanResult.txPowerLevel).c_str() : "-");
    if (connectRequested)
    {
      ble.connect(scanResult);
    }
  });

  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("CENTRAL_CONNECTED id=%u\n", connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("CENTRAL_DISCONNECTED id=%u reason=0x%02x\n",
      connection.id, connection.disconnectReason);
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's' || command == 'c')
    {
      reported = false;
      connectRequested = command == 'c';
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
  }

  ble.update();
  delay(1);
}
