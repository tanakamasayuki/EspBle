#include <EspBle.h>

static constexpr const char *TEST_SERVICE_UUID = "98d46f50-c2a7-4a71-9003-636f65786973";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "98d46f51-c2a7-4a71-9003-636f65786973";

EspBle ble;
EspBleGattCharacteristic testCharacteristic;
bool subscribed = false;

void setup()
{
  Serial.begin(115200);
  delay(500);

  auto &server = ble.gattServer();
  EspBleGattCharacteristicConfig characteristicConfig;
  characteristicConfig.readable = true;
  characteristicConfig.writable = true;
  characteristicConfig.notifiable = true;
  const EspBleGattService service = server.addService(TEST_SERVICE_UUID);
  testCharacteristic = server.addCharacteristic(
    service, TEST_CHARACTERISTIC_UUID, characteristicConfig);
  if (!service.valid() || !testCharacteristic.valid() ||
      !server.setValue(testCharacteristic, String("s3-ready")))
  {
    Serial.println("GATT_CONFIG_FAILED");
    return;
  }

  server.onWritten([](const EspBleGattWrite &write) {
    Serial.printf("SERVER_WRITE value=%s\n", write.value.c_str());
  });
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    if (subscription.characteristic != testCharacteristic) return;
    subscribed = subscription.notifications;
    Serial.printf("SERVER_SUBSCRIBED %u\n", subscribed ? 1 : 0);
  });

  EspBleConfig config;
  config.deviceName = "EspBle Coexistence Peer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.advertising().setName("EspBle Coexistence Peer");
  ble.advertising().addServiceUuid(TEST_SERVICE_UUID);
  Serial.println(ble.advertising().start() ? "ADVERTISING 1" : "ADVERTISING 0");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'n')
    {
      const bool accepted = subscribed &&
        ble.gattServer().notify(testCharacteristic, String("s3-notify"));
      Serial.printf("NOTIFY_ACCEPTED %u\n", accepted ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
