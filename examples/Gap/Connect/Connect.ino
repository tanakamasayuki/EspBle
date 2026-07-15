#include <EspBle.h>

// Replace this with a service UUID advertised by the Peripheral to connect to.
static constexpr const char *TARGET_SERVICE_UUID = "5266f727-49d7-4eaf-a6f1-636f6e6e6563";

EspBle ble;
bool connectionRequested = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Central";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("Connected to %s (id=%u)\n", connection.peerAddress.c_str(), connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("Disconnected (id=%u)\n", connection.id);
    connectionRequested = false;
  });
  ble.onConnectionFailed([](const EspBleConnectionFailure &failure) {
    Serial.printf("Connection failed: %s\n", failure.detail.c_str());
    connectionRequested = false;
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(TARGET_SERVICE_UUID))
    {
      return;
    }

    ble.scanner().stop();
    connectionRequested = ble.connect(scanResult);
    if (!connectionRequested)
    {
      Serial.printf("Connection request rejected: %s\n", ble.lastErrorDetail().c_str());
    }
  });

  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  if (!ble.scanner().start(scanConfig))
  {
    Serial.printf("Scan start failed: %s\n", ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  ble.update();
  delay(1);
}
