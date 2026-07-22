// en: SubscribeClient - connect to the Gatt/NotifyServer example, subscribe to
//     notifications, and print each received value.
// ja: SubscribeClient - Gatt/NotifyServer example へ接続し、Notificationを購読して
//     受信値を表示する。
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "71756360-5fa4-43bc-9003-6e6f74696679";
static constexpr const char *CHARACTERISTIC_UUID = "71756361-5fa4-43bc-9003-6e6f74696679";

EspBle ble;
bool connectionRequested = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Subscribe Client";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Subscribe after connecting (4th arg true = notifications, false = indications).
  // ja: 接続完了後にNotificationを購読する（第4引数true = Notification、false = Indication）。
  ble.onConnected([](const EspBleConnection &connection) {
    if (!ble.subscribe(connection.id, SERVICE_UUID, CHARACTERISTIC_UUID, true))
    {
      Serial.printf("Subscribe request failed: %s\n", ble.lastErrorDetail().c_str());
    }
  });
  // en: Subscription (CCCD write) completion.
  // ja: 購読（CCCD書込み）の完了。
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Subscribe failed: %s\n", result.detail.c_str());
    }
  });
  // en: A received notification (payload is copied).
  // ja: 受信したNotification（payloadはcopy済み）。
  ble.onNotification([](const EspBleGattNotification &notification) {
    Serial.printf("Notification: %s\n", notification.value.c_str());
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
  ble.scanner().start(scanConfig);
}

void loop()
{
  // en: Subscription and notification events are delivered from this update().
  // ja: 購読完了・Notificationイベントはこの update() から配送される。
  ble.update();
  delay(1);
}
