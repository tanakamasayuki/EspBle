#include <EspBle.h>

static constexpr const char *NUS_SERVICE_UUID =
  "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr const char *NUS_RX_UUID =
  "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr const char *NUS_TX_UUID =
  "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

EspBle ble;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;
bool ready = false;

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(50);

  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    ble.subscribe(connection.id, NUS_SERVICE_UUID, NUS_TX_UUID);
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    ready = result.success;
    Serial.printf("NUS ready: %u\n", ready ? 1 : 0);
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    Serial.printf("TX accepted: %u\n", result.success ? 1 : 0);
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.characteristicUuid.equalsIgnoreCase(NUS_TX_UUID))
    {
      Serial.printf("RX: %s\n", notification.value.c_str());
    }
  });
  ble.onDisconnected([](const EspBleConnection &) {
    connectionId = 0;
    connectionRequested = false;
    ready = false;
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(NUS_SERVICE_UUID)) return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
  });

  EspBleScanConfig scan;
  scan.active = true;
  ble.scanner().start(scan);
}

void loop()
{
  if (ready && Serial.available() > 0)
  {
    String line = Serial.readStringUntil('\n');
    if (!line.isEmpty())
    {
      ble.writeCharacteristic(
        connectionId, NUS_SERVICE_UUID, NUS_RX_UUID, line, false);
    }
  }
  ble.update();
  delay(1);
}
