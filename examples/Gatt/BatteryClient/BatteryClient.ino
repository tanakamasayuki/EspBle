#include <EspBle.h>

static constexpr const char *BATTERY_SERVICE_UUID = "180f";
static constexpr const char *BATTERY_LEVEL_UUID = "2a19";

EspBle ble;
bool connectionRequested = false;

void setup()
{
  Serial.begin(115200);

  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    ble.readCharacteristic(connection.id, BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.success || result.value.length() != 1)
    {
      Serial.printf("Battery read failed: %s\n", result.detail.c_str());
      return;
    }
    Serial.printf("Battery: %u%%\n", static_cast<uint8_t>(result.value[0]));
    ble.subscribe(result.connectionId, BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID);
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("Battery subscription: %s\n", result.success ? "ready" : "failed");
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.serviceUuid.equalsIgnoreCase(BATTERY_SERVICE_UUID) &&
        notification.characteristicUuid.equalsIgnoreCase(BATTERY_LEVEL_UUID) &&
        notification.value.length() == 1)
    {
      Serial.printf("Battery changed: %u%%\n",
        static_cast<uint8_t>(notification.value[0]));
    }
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(BATTERY_SERVICE_UUID)) return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
  });

  EspBleScanConfig scan;
  scan.active = true;
  ble.scanner().start(scan);
}

void loop()
{
  ble.update();
  delay(1);
}
