// en: Subscribe to IndicateServer's indications
// ja: IndicateServerのIndicationを購読
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "43a2c560-71b4-49bd-9003-696e64696361";
static constexpr const char *CHARACTERISTIC_UUID = "43a2c561-71b4-49bd-9003-696e64696361";

EspBle ble;
bool connectionRequested = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Indicate Client";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  ble.onConnected([](const EspBleConnection &connection) {
    // en: notifications=false subscribes to indications (writes 0x0002 to the CCCD).
    // ja: notifications=false でIndicationを購読する（CCCDへ0x0002を書き込む）。
    if (!ble.subscribe(connection.id, SERVICE_UUID, CHARACTERISTIC_UUID, false))
    {
      Serial.printf("Subscribe request failed: %s\n", ble.lastErrorDetail().c_str());
    }
  });
  // en: Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
  // ja: result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Subscribe failed: %s\n", result.detail.c_str());
    }
  });
  // en: onNotification receives both; notification.indication tells them apart.
  // ja: onNotification は両方を受信し、notification.indication で区別する。
  ble.onNotification([](const EspBleGattNotification &notification) {
    Serial.printf(
      "%s: %s\n",
      notification.indication ? "Indication" : "Notification",
      notification.value.c_str());
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

  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  ble.scanner().start(scanConfig);
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
