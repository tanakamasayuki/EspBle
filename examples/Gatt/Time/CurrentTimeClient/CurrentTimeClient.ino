// en: Current Time decoding and notification subscription
// ja: Current TimeのdecodeとNotification購読
#include <EspBle.h>

static constexpr const char *CURRENT_TIME_SERVICE_UUID = "1805";
static constexpr const char *CURRENT_TIME_UUID = "2a2b";

EspBle ble;
bool connectionRequested = false;

static void printCurrentTime(const String &value, const char *prefix)
{
  if (value.length() != 10)
  {
    Serial.printf("%s has invalid length: %u\n", prefix,
      static_cast<unsigned>(value.length()));
    return;
  }
  const uint16_t year = static_cast<uint8_t>(value[0]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[1])) << 8);
  Serial.printf("%s: %04u-%02u-%02u %02u:%02u:%02u weekday=%u fraction=%u/256 reason=0x%02x\n",
    prefix, year,
    static_cast<uint8_t>(value[2]), static_cast<uint8_t>(value[3]),
    static_cast<uint8_t>(value[4]), static_cast<uint8_t>(value[5]),
    static_cast<uint8_t>(value[6]), static_cast<uint8_t>(value[7]),
    static_cast<uint8_t>(value[8]), static_cast<uint8_t>(value[9]));
}

void setup()
{
  Serial.begin(115200);
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
    ble.readCharacteristic(connection.id, CURRENT_TIME_SERVICE_UUID, CURRENT_TIME_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Current Time read failed: %s\n", result.detail.c_str());
      return;
    }
    printCurrentTime(result.value, "Current Time");
    ble.subscribe(result.connectionId, CURRENT_TIME_SERVICE_UUID, CURRENT_TIME_UUID);
  });
  // en: Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
  // ja: result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("Current Time subscription: %s\n",
      result.success ? "ready" : "failed");
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.serviceUuid.equalsIgnoreCase(CURRENT_TIME_SERVICE_UUID) &&
        notification.characteristicUuid.equalsIgnoreCase(CURRENT_TIME_UUID))
    {
      printCurrentTime(notification.value, "Current Time changed");
    }
  });
  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(CURRENT_TIME_SERVICE_UUID)) return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
  });

  EspBleScanConfig scan;
  scan.active = true;
  ble.scanner().start(scan);
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
