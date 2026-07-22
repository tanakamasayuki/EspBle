// en: IndicateServer - indication variant of NotifyServer: each value is delivered with
//     an ATT confirmation from the client, and the result arrives via onSent().
//     Pair with Gatt/IndicateClient.
// ja: IndicateServer - NotifyServerのIndication版。各値はClientからのATT確認応答つきで
//     配信され、結果は onSent() で届く。Gatt/IndicateClient と組み合わせる。
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "43a2c560-71b4-49bd-9003-696e64696361";
static constexpr const char *CHARACTERISTIC_UUID = "43a2c561-71b4-49bd-9003-696e64696361";

EspBle ble;
bool hasIndicationSubscriber = false; // en: is there an indication subscriber / ja: Indication購読者がいるか
uint32_t lastIndication = 0;
uint32_t counter = 0;

void setup()
{
  Serial.begin(115200);

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig counterConfig;
  counterConfig.readable = true;
  counterConfig.indicatable = true; // en: add Indicate property and CCCD / ja: Indicate propertyとCCCDを付与
  gattServer.addService(SERVICE_UUID);
  gattServer.addCharacteristic(SERVICE_UUID, CHARACTERISTIC_UUID, counterConfig);
  gattServer.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    hasIndicationSubscriber = subscription.indications;
  });
  gattServer.onSent([](const EspBleGattSendResult &result) {
    // en: For indications, success means the client confirmed the delivery.
    // ja: Indicationでは success はClientが配信を確認したことを意味する。
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

  // en: Send the counter every 2 seconds while a subscriber exists.
  // ja: 購読者がいる間、2秒ごとにカウンタを送る。
  if (hasIndicationSubscriber && millis() - lastIndication >= 2000)
  {
    lastIndication = millis();
    const String value = String(++counter);
    // en: Accepted synchronously; the confirmation arrives later via onSent().
    // ja: 同期的に受理され、確認はあとから onSent() で届く。
    ble.gattServer().indicate(SERVICE_UUID, CHARACTERISTIC_UUID, value);
  }
  delay(1);
}
