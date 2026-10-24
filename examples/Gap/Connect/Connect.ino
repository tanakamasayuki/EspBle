// en: Scan for a service UUID and connect; async connect/disconnect/failure events
// ja: Service UUIDをscanして接続。非同期の接続/切断/失敗イベント
#include <EspBle.h>

// en: Replace with the service UUID advertised by the peripheral to connect to.
// ja: 接続したいPeripheralがadvertiseするService UUIDに書き換える。
static constexpr const char *TARGET_SERVICE_UUID = "5266f727-49d7-4eaf-a6f1-636f6e6e6563";

EspBle ble;
// en: guard against duplicate requests
// ja: 二重接続要求を防ぐフラグ
bool connectionRequested = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Central";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Connected. connection.id is a stable per-connection id, distinct from the backend handle.
  // ja: 接続成功。connection.id はbackend handleとは別の、接続ごとに安定した識別子。
  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("Connected to %s (id=%u)\n", connection.peerAddress.c_str(), connection.id);
  });
  // en: Disconnected. The same id as onConnected is passed.
  // ja: 切断。onConnected と同じ id が渡る。
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("Disconnected (id=%u)\n", connection.id);
    // en: allow retry on the next scan result
    // ja: 次のscan resultで再試行できるように戻す
    connectionRequested = false;
  });
  // en: Asynchronous connection failure (unreachable, timeout, etc.).
  // ja: 非同期の接続失敗（到達不能・timeout等）。
  ble.onConnectionFailed([](const EspBleConnectionFailure &failure) {
    Serial.printf("Connection failed: %s\n", failure.detail.c_str());
    connectionRequested = false;
  });

  // en: Receive scan results one by one and connect when the target service UUID matches.
  // ja: scan結果を1件ずつ受け取り、目的のService UUIDなら接続する。
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(TARGET_SERVICE_UUID))
    {
      // en: already requested, or not the target
      // ja: 既に接続要求済み、または対象外
      return;
    }

    // en: stop scanning before connecting
    // ja: 接続前にscanを止める
    ble.scanner().stop();
    // en: true if accepted (completion via event)
    // ja: 受理されれば true（完了はイベントで）
    connectionRequested = ble.connect(scanResult);
    if (!connectionRequested)
    {
      Serial.printf("Connection request rejected: %s\n", ble.lastErrorDetail().c_str());
    }
  });

  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  if (!ble.scanner().start(scanConfig))
  {
    Serial.printf("Scan start failed: %s\n", ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  // en: Connect/disconnect/failure events are delivered from this update().
  // ja: 接続・切断・失敗のイベントはこの update() から配送される。
  ble.update();
  delay(1);
}
