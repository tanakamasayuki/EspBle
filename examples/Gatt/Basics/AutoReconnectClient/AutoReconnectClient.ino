// en: AutoReconnectClient - connect to the Gatt/NotifyServer example and
//     subscribe once. With auto-reconnect on and persistent subscriptions
//     (on by default), if the link drops the library reconnects to the same
//     peer and restores the subscription on its own, so notifications resume
//     with no extra code.
// ja: AutoReconnectClient - Gatt/NotifyServer example へ接続し一度だけ購読する。
//     auto-reconnectを有効にすると、persistent subscription（既定on）と併せて、
//     切断時にライブラリが同じpeerへ自動再接続し購読も自動復元するため、追加の
//     コードなしでNotificationが再開する。
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
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Keep this peer connected: reconnect automatically on an unexpected drop.
  // ja: このpeerとの接続を維持する。想定外の切断時に自動で再接続する。
  ble.setAutoReconnect(true);

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
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (result.success)
    {
      subscribed = true;
      Serial.println("Subscription active (restored automatically after a reconnect).");
    }
  });
  ble.onDisconnected([](const EspBleConnection &) {
    // en: No reconnect code needed here - the library handles it. Allow the
    //     scanner path to run again only if it is ever needed for the first link.
    // ja: 再接続コードは不要（ライブラリが処理する）。
    Serial.println("Disconnected - auto-reconnect will restore the link.");
    connectionRequested = false;
  });
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
