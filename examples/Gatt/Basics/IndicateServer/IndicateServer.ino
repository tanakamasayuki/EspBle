// en: Acknowledged indications with onSent() delivery confirmation
// ja: 確認応答つきのIndication配信とonSent()での配信確認
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "43a2c560-71b4-49bd-9003-696e64696361";
static constexpr const char *CHARACTERISTIC_UUID = "43a2c561-71b4-49bd-9003-696e64696361";

EspBle ble;
EspBleGattService service;
EspBleGattCharacteristic characteristic;
// en: is there an indication subscriber
// ja: Indication購読者がいるか
bool hasIndicationSubscriber = false;
uint32_t lastIndication = 0;
uint32_t counter = 0;

void setup()
{
  Serial.begin(115200);

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig counterConfig;
  counterConfig.readable = true;
  // en: add Indicate property and CCCD
  // ja: Indicate propertyとCCCDを付与
  counterConfig.indicatable = true;
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  service = gattServer.addService(SERVICE_UUID);
  characteristic = gattServer.addCharacteristic(service, CHARACTERISTIC_UUID, counterConfig);
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
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
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
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();

  // en: Send the counter every 2 seconds while a subscriber exists.
  // ja: 購読者がいる間、2秒ごとにカウンタを送る。
  if (hasIndicationSubscriber && millis() - lastIndication >= 2000)
  {
    lastIndication = millis();
    const String value = String(++counter);
    // en: Accepted synchronously; the confirmation arrives later via onSent().
    // ja: 同期的に受理され、確認はあとから onSent() で届く。
    ble.gattServer().indicate(characteristic, value);
  }
  delay(1);
}
