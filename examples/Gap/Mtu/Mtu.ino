// en: Preferred-MTU exchange and notification payload limits
// ja: 希望MTUの交換とNotification payload上限
#include <EspBle.h>

// en: Matches the service UUID of the Gatt/NotifyServer example.
// ja: Gatt/NotifyServer example のService UUIDに合わせてある。
static constexpr const char *TARGET_SERVICE_UUID = "71756360-5fa4-43bc-9003-6e6f74696679";

EspBle ble;
bool connectionRequested = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle MTU Central";
  // en: preferred ATT MTU (23-517; out of range is rejected by begin())
  // ja: 希望ATT MTU（23〜517。範囲外は begin() が拒否）
  config.preferredMtu = 185;
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
    // en: connection.mtu is the snapshot taken at connection; maximumNotificationPayload()
    //     is mtu-3 (excluding the ATT notification header).
    // ja: connection.mtu は接続完了時のsnapshot。maximumNotificationPayload() は
    //     mtu-3（ATT notification header分を除いた値）。
    Serial.printf(
      "Connected with MTU %u (notification payload up to %u bytes)\n",
      connection.mtu,
      static_cast<unsigned>(connection.maximumNotificationPayload()));
  });
  ble.onMtuChanged([](const EspBleMtuChanged &event) {
    Serial.printf("MTU changed from %u to %u\n", event.previousMtu, event.connection.mtu);
  });
  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(TARGET_SERVICE_UUID))
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
