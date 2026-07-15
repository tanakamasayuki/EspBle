#include <EspBle.h>

// This matches the Gatt/NotifyServer example.
static constexpr const char *TARGET_SERVICE_UUID = "71756360-5fa4-43bc-9003-6e6f74696679";

EspBle ble;
bool connectionRequested = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle MTU Central";
  config.preferredMtu = 185;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf(
      "Connected with MTU %u (notification payload up to %u bytes)\n",
      connection.mtu,
      static_cast<unsigned>(connection.maximumNotificationPayload()));
  });
  ble.onMtuChanged([](const EspBleMtuChanged &event) {
    Serial.printf("MTU changed from %u to %u\n", event.previousMtu, event.connection.mtu);
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(TARGET_SERVICE_UUID))
    {
      return;
    }
    ble.scanner().stop();
    connectionRequested = ble.connect(scanResult);
  });

  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  ble.scanner().start(scanConfig);
}

void loop()
{
  ble.update();
  delay(1);
}
