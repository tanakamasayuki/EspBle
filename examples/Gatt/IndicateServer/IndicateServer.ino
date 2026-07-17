#include <EspBle.h>

// Indication variant of NotifyServer: each value is delivered with an ATT
// confirmation from the client, and the result arrives via onSent().
// Pair with Gatt/IndicateClient.

static constexpr const char *SERVICE_UUID = "43a2c560-71b4-49bd-9003-696e64696361";
static constexpr const char *CHARACTERISTIC_UUID = "43a2c561-71b4-49bd-9003-696e64696361";

EspBle ble;
bool hasIndicationSubscriber = false;
uint32_t lastIndication = 0;
uint32_t counter = 0;

void setup()
{
  Serial.begin(115200);

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig counterConfig;
  counterConfig.readable = true;
  counterConfig.indicatable = true;
  gattServer.addService(SERVICE_UUID);
  gattServer.addCharacteristic(SERVICE_UUID, CHARACTERISTIC_UUID, counterConfig);
  gattServer.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    hasIndicationSubscriber = subscription.indications;
  });
  gattServer.onSent([](const EspBleGattSendResult &result) {
    // For indications, success means the client confirmed the delivery.
    Serial.printf(
      "Indication %s: %s\n",
      result.success ? "confirmed" : "failed",
      result.value.c_str());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Indicate Server";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Indicate Server");
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.start();
}

void loop()
{
  ble.update();

  if (hasIndicationSubscriber && millis() - lastIndication >= 2000)
  {
    lastIndication = millis();
    const String value = String(++counter);
    // Accepted synchronously; the confirmation arrives later via onSent().
    ble.gattServer().indicate(SERVICE_UUID, CHARACTERISTIC_UUID, value);
  }
  delay(1);
}
