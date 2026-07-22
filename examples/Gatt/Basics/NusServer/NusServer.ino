#include <EspBle.h>

static constexpr const char *NUS_SERVICE_UUID =
  "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr const char *NUS_RX_UUID =
  "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr const char *NUS_TX_UUID =
  "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

EspBle ble;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig rxConfig;
  rxConfig.writable = true;
  rxConfig.writableWithoutResponse = true;
  EspBleGattCharacteristicConfig txConfig;
  txConfig.notifiable = true;

  auto &server = ble.gattServer();
  if (!server.addService(NUS_SERVICE_UUID) ||
      !server.addCharacteristic(NUS_SERVICE_UUID, NUS_RX_UUID, rxConfig) ||
      !server.addCharacteristic(NUS_SERVICE_UUID, NUS_TX_UUID, txConfig))
  {
    Serial.printf("NUS configuration failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(NUS_RX_UUID)) return;
    Serial.printf("RX: %s\n", write.value.c_str());
    const bool echoed = ble.gattServer().notify(
      NUS_SERVICE_UUID, NUS_TX_UUID, write.value);
    Serial.printf("Echo accepted: %u\n", echoed ? 1 : 0);
  });
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    if (subscription.characteristicUuid.equalsIgnoreCase(NUS_TX_UUID))
    {
      Serial.printf("TX notifications: %u\n", subscription.notifications ? 1 : 0);
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle NUS";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle NUS");
  ble.advertising().addServiceUuid(NUS_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  ble.update();
  delay(1);
}
