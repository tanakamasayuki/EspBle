// en: NUS-compatible TX subscription and RX Write Without Response
// ja: NUS互換TX購読とRX Write Without Response
#include <EspBle.h>

static constexpr const char *NUS_SERVICE_UUID =
  "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr const char *NUS_RX_UUID =
  "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr const char *NUS_TX_UUID =
  "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

EspBle ble;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;
bool ready = false;

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(50);

  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    ble.subscribe(connection.id, NUS_SERVICE_UUID, NUS_TX_UUID);
  });
  // en: Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
  // ja: result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
  ble.onSubscribed([](const EspBleGattResult &result) {
    ready = result.success;
    Serial.printf("NUS ready: %u\n", ready ? 1 : 0);
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    Serial.printf("TX accepted: %u\n", result.success ? 1 : 0);
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.characteristicUuid.equalsIgnoreCase(NUS_TX_UUID))
    {
      Serial.printf("RX: %s\n", notification.value.c_str());
    }
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &) {
    connectionId = 0;
    connectionRequested = false;
    ready = false;
  });
  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(NUS_SERVICE_UUID)) return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
  });

  EspBleScanConfig scan;
  scan.active = true;
  ble.scanner().start(scan);
}

void loop()
{
  if (ready && Serial.available() > 0)
  {
    String line = Serial.readStringUntil('\n');
    if (!line.isEmpty())
    {
      ble.writeCharacteristic(
        connectionId, NUS_SERVICE_UUID, NUS_RX_UUID, line, false);
    }
  }
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
