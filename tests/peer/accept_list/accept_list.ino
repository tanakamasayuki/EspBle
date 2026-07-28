// Central for the accept_list peer test: scan for the peripheral and try to
// connect. The connection must fail while the peripheral filters connections
// against its accept list, and succeed once the policy is open again.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "FEAD";
// Shorter than the library default so a blocked attempt reports back quickly.
static constexpr uint32_t CONNECT_TIMEOUT_MS = 4000;

EspBle ble;
bool connectRequested = false;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Accept List Central";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectRequested || !scanResult.advertisesService(SERVICE_UUID)) return;
    connectRequested = true;
    ble.scanner().stop();
    Serial.printf("TARGET_FOUND %s\n", scanResult.address.c_str());
    if (!ble.connect(scanResult, CONNECT_TIMEOUT_MS))
    {
      Serial.printf("CONNECT_REJECTED %s\n", ble.lastErrorName());
    }
  });

  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("CENTRAL_CONNECTED id=%u\n", connection.id);
  });
  ble.onConnectionFailed([](const EspBleConnectionFailure &failure) {
    Serial.printf("CENTRAL_CONNECT_FAILED %s\n", failure.detail.c_str());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("CENTRAL_DISCONNECTED id=%u\n", connection.id);
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'c')
    {
      connectRequested = false;
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'd')
    {
      Serial.println(ble.disconnect(1) ? "DISCONNECT_REQUESTED" : "DISCONNECT_FAILED");
    }
  }

  ble.update();
  delay(1);
}
