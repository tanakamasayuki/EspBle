// en: NotifyServer - a GATT server that notifies a counter value once per second, but
//     only while at least one client subscribes. Pair with Gatt/SubscribeClient.
// ja: NotifyServer - カウンタ値を1秒ごとにNotificationで送るGATT Server。
//     ただしNotificationを購読しているClientがいる間だけ送る。Gatt/SubscribeClient と組み合わせる。
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "71756360-5fa4-43bc-9003-6e6f74696679";
static constexpr const char *CHARACTERISTIC_UUID = "71756361-5fa4-43bc-9003-6e6f74696679";

EspBle ble;
bool hasNotificationSubscriber = false; // en: is there a CCCD subscriber / ja: CCCD購読者がいるか
uint32_t lastNotification = 0;          // en: last send time / ja: 前回送信時刻
uint32_t counter = 0;                   // en: counter value to send / ja: 送信するカウンタ値

void setup()
{
  Serial.begin(115200);

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig counterConfig;
  counterConfig.readable = true;
  counterConfig.notifiable = true; // en: add Notify property and CCCD / ja: Notify propertyとCCCDを付与
  gattServer.addService(SERVICE_UUID);
  gattServer.addCharacteristic(SERVICE_UUID, CHARACTERISTIC_UUID, counterConfig);

  // en: Track CCCD subscription state so we notify only while a subscriber exists.
  // ja: CCCDの購読状態変化。購読者がいる間だけ notify するために追跡する。
  gattServer.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    hasNotificationSubscriber = subscription.notifications;
  });
  // en: Asynchronous send result.
  // ja: 非同期の送信結果。
  gattServer.onSent([](const EspBleGattSendResult &result) {
    if (!result.success)
    {
      Serial.printf("Notification failed: %s\n", result.detail.c_str());
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Notify Server";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Notify Server");
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.start();
}

void loop()
{
  ble.update();

  // en: Send the counter every second, only while there is a subscriber.
  // ja: 購読者がいるときだけ、1秒ごとにカウンタを送る。
  if (hasNotificationSubscriber && millis() - lastNotification >= 1000)
  {
    lastNotification = millis();
    const String value = String(++counter);
    // en: notify() is accepted synchronously and sent to all subscribed connections.
    // ja: notify() は同期的に受理され、購読中の全接続へ送信される。
    ble.gattServer().notify(SERVICE_UUID, CHARACTERISTIC_UUID, value);
  }
  delay(1);
}
