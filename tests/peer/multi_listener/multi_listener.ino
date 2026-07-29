// multi_listener DUT: GATT client that writes to the peer, and whose own write
// -completion event carries a primary callback plus additional listeners
// (EspBle::addCharacteristicWrittenListener). Covers the EspBle-owned listener
// list and removeGattListener(), the counterpart of the server-side list the
// peer_device sketch exercises.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "FEAE";
static constexpr const char *CHARACTERISTIC_UUID = "2ae3";

EspBle ble;
EspBleConnectionId connectionId = 0;
bool connectRequested = false;

unsigned primaryCount = 0;
unsigned firstCount = 0;
unsigned secondCount = 0;
EspBleListenerId firstListener = EspBleInvalidListenerId;
EspBleListenerId secondListener = EspBleInvalidListenerId;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle MultiListener Central";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    ++primaryCount;
    Serial.printf("WRITE_DONE success=%u\n", result.success ? 1 : 0);
  });
  firstListener = ble.addCharacteristicWrittenListener([](const EspBleGattResult &) {
    ++firstCount;
  });
  secondListener = ble.addCharacteristicWrittenListener([](const EspBleGattResult &) {
    ++secondCount;
  });
  Serial.printf("LISTENERS_REGISTERED first=%u second=%u\n",
    static_cast<unsigned>(firstListener), static_cast<unsigned>(secondListener));

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("CENTRAL_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
    // Writing by UUID needs the characteristic discovered first.
    ble.discoverCharacteristic(connection.id, SERVICE_UUID, CHARACTERISTIC_UUID);
  });
  ble.onCharacteristicDiscovered([](const EspBleGattResult &result) {
    Serial.printf("CENTRAL_READY success=%u\n", result.success ? 1 : 0);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("CENTRAL_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
    connectionId = 0;
    connectRequested = false;
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectRequested || !scanResult.advertisesService(SERVICE_UUID)) return;
    connectRequested = true;
    ble.scanner().stop();
    Serial.println(ble.connect(scanResult) ? "CONNECT_REQUESTED" : "CONNECT_REQUEST_FAILED");
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'c')
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'w' && connectionId != 0)
    {
      const bool queued = ble.writeCharacteristic(
        connectionId, SERVICE_UUID, CHARACTERISTIC_UUID, String("hello"));
      Serial.println(queued ? "WRITE_REQUESTED" : "WRITE_REQUEST_FAILED");
    }
    else if (command == 's')
    {
      Serial.printf("WRITE_STATE primary=%u first=%u second=%u\n",
        primaryCount, firstCount, secondCount);
    }
    else if (command == 'z')
    {
      primaryCount = 0;
      firstCount = 0;
      secondCount = 0;
      Serial.println("WRITE_STATE_RESET");
    }
    else if (command == 'r')
    {
      const bool removed = ble.removeGattListener(secondListener);
      Serial.printf("LISTENER_REMOVED success=%u id=%u\n",
        removed ? 1 : 0, static_cast<unsigned>(secondListener));
      secondListener = EspBleInvalidListenerId;
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_FAILED");
    }
  }

  ble.update();
  delay(1);
}
