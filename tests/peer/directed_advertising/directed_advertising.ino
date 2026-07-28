#include <EspBle.h>

static constexpr const char *TEST_SERVICE_UUID = "3d9b1c40-6f2e-4a8b-9f31-64697265637a";

EspBle ble;
EspBleConnectionId connectionId = 0;
bool connectRequested = false;
String peripheralAddress;
EspBleAddressType peripheralAddressType = EspBleAddressType::Public;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Directed Central";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    peripheralAddress = connection.peerAddress;
    peripheralAddressType = connection.peerAddressType;
    Serial.printf(
      "CENTRAL_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectRequested = false;
    Serial.printf("CENTRAL_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectRequested || !scanResult.advertisesService(TEST_SERVICE_UUID))
    {
      return;
    }
    ble.scanner().stop();
    connectRequested = ble.connect(scanResult);
    Serial.println(connectRequested ? "CONNECT_REQUESTED" : "CONNECT_REQUEST_FAILED");
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's' && !connectRequested)
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'c' && !connectRequested && peripheralAddress.length() > 0)
    {
      // A directed advertisement carries no payload, so it cannot be matched by
      // service UUID: the central connects to the address it learned earlier.
      connectRequested = ble.connect(peripheralAddress.c_str(), peripheralAddressType);
      Serial.println(connectRequested ? "CONNECT_REQUESTED" : "CONNECT_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }

  ble.update();
  delay(1);
}
