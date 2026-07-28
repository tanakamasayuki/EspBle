#include <EspBle.h>

static constexpr const char *NUS_SERVICE_UUID =
  "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr const char *NUS_RX_UUID =
  "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr const char *NUS_TX_UUID =
  "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

EspBle ble;

EspBleGattService nusServiceService;
EspBleGattCharacteristic nusRxCharacteristic;
EspBleGattCharacteristic nusTxCharacteristic;
void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig rxConfig;
  rxConfig.writable = true;
  rxConfig.writableWithoutResponse = true;
  EspBleGattCharacteristicConfig txConfig;
  txConfig.notifiable = true;

  auto &server = ble.gattServer();
  if (!(nusServiceService = server.addService(NUS_SERVICE_UUID)).valid() ||
      !(nusRxCharacteristic = server.addCharacteristic(nusServiceService, NUS_RX_UUID, rxConfig)).valid() ||
      !(nusTxCharacteristic = server.addCharacteristic(nusServiceService, NUS_TX_UUID, txConfig)).valid())
  {
    Serial.printf("NUS configuration failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(NUS_RX_UUID)) return;
    Serial.printf("RX: %s\n", write.value.c_str());
    const bool echoed = ble.gattServer().notify(nusTxCharacteristic, write.value);
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
