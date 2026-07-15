#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "10da4dd0-8eaa-4c69-9003-676174747277";
static constexpr const char *CHARACTERISTIC_UUID = "10da4dd1-8eaa-4c69-9003-676174747277";

EspBle ble;
bool connectionRequested = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle GATT Client";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    if (!ble.discoverCharacteristic(connection.id, SERVICE_UUID, CHARACTERISTIC_UUID))
    {
      Serial.printf("Discovery request failed: %s\n", ble.lastErrorDetail().c_str());
    }
  });
  ble.onCharacteristicDiscovered([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Discovery failed: %s\n", result.detail.c_str());
      return;
    }
    ble.readCharacteristic(result.connectionId, SERVICE_UUID, CHARACTERISTIC_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Read failed: %s\n", result.detail.c_str());
      return;
    }
    Serial.printf("Read: %s\n", result.value.c_str());
    ble.writeCharacteristic(
      result.connectionId,
      SERVICE_UUID,
      CHARACTERISTIC_UUID,
      String("hello from Central"),
      true);
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    Serial.println(result.success ? "Write complete" : "Write failed");
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(SERVICE_UUID))
    {
      return;
    }
    ble.scanner().stop();
    connectionRequested = ble.connect(scanResult);
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
