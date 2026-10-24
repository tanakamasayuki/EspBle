// en: Auto-reconnect + persistent subscription: notifications resume after a drop
// ja: auto-reconnect + persistent subscription: 切断後にNotificationが自動再開
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "71756360-5fa4-43bc-9003-6e6f74696679";
static constexpr const char *CHARACTERISTIC_UUID = "71756361-5fa4-43bc-9003-6e6f74696679";

EspBle ble;
bool connectionRequested = false;
bool subscribed = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle AutoReconnect Client";
  // en: persistentSubscriptions defaults to true; shown here for clarity.
  // ja: persistentSubscriptionsは既定true。ここでは明示のために記述。
  config.persistentSubscriptions = true;
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Keep this peer connected: reconnect automatically on an unexpected drop.
  // ja: このpeerとの接続を維持する。想定外の切断時に自動で再接続する。
  ble.setAutoReconnect(true);

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("Connected to %s\n", connection.peerAddress.c_str());
    // en: Subscribe once. On a later reconnect the library restores it, so this
    //     guard avoids issuing a duplicate subscribe.
    // ja: 購読は一度だけ。再接続時はライブラリが復元するため、この判定で二重購読を避ける。
    if (!subscribed)
    {
      ble.subscribe(connection.id, SERVICE_UUID, CHARACTERISTIC_UUID, true);
    }
  });
  // en: Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
  // ja: result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (result.success)
    {
      subscribed = true;
      Serial.println("Subscription active (restored automatically after a reconnect).");
    }
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &) {
    // en: No reconnect code needed here - the library handles it. Allow the
    //     scanner path to run again only if it is ever needed for the first link.
    // ja: 再接続コードは不要（ライブラリが処理する）。
    Serial.println("Disconnected - auto-reconnect will restore the link.");
    connectionRequested = false;
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    Serial.printf("Notification: %s\n", notification.value.c_str());
  });
  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(SERVICE_UUID))
    {
      return;
    }
    ble.scanner().stop();
    connectionRequested = ble.connect(scanResult);
  });

  // en: The initial discovery only needs a scan for the first connection; the
  //     auto-reconnect afterwards uses the remembered address directly.
  // ja: 最初の接続だけscanで探す。以降のauto-reconnectは記憶したaddressを直接使う。
  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  ble.scanner().start(scanConfig);
}

void loop()
{
  // en: Connection, subscription, notification, and auto-reconnect are all
  //     driven from this update().
  // ja: 接続・購読・Notification・auto-reconnectはすべてこの update() から駆動される。
  ble.update();
  delay(1);
}
