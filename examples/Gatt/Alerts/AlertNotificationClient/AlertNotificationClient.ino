// en: Control Point "Notify New Alert Immediately" and New Alert decoding
// ja: Control Point「Notify New Alert Immediately」とNew Alertのdecode
#include <EspBle.h>

static constexpr const char *ANS_SERVICE_UUID = "1811";
static constexpr const char *SUPPORTED_NEW_ALERT_CATEGORY_UUID = "2a47";
static constexpr const char *NEW_ALERT_UUID = "2a46";
static constexpr const char *ALERT_CONTROL_POINT_UUID = "2a44";

EspBle ble;

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
    ble.readCharacteristic(connection.id, ANS_SERVICE_UUID, SUPPORTED_NEW_ALERT_CATEGORY_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(SUPPORTED_NEW_ALERT_CATEGORY_UUID) &&
        result.success && result.value.length() == 2)
    {
      const uint16_t mask = static_cast<uint8_t>(result.value[0]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(result.value[1])) << 8);
      Serial.printf("Supported categories: 0x%04x\n", mask);
      ble.subscribe(result.connectionId, ANS_SERVICE_UUID, NEW_ALERT_UUID);
    }
  });
  // en: Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
  // ja: result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (result.success)
    {
      // en: Notify New Alert Immediately (command 2) for Email (category 1).
      // ja: Email（category 1）に対してNotify New Alert Immediately（command 2）。
      const uint8_t request[2] = {0x02, 0x01};
      ble.writeCharacteristic(
        result.connectionId, ANS_SERVICE_UUID, ALERT_CONTROL_POINT_UUID, request, sizeof(request), true);
    }
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(NEW_ALERT_UUID) ||
        notification.value.length() < 2)
      return;
    const String &value = notification.value;
    const uint8_t category = static_cast<uint8_t>(value[0]);
    const uint8_t count = static_cast<uint8_t>(value[1]);
    Serial.printf("New Alert: category %u, count %u, text \"%s\"\n",
      category, count, value.substring(2).c_str());
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(ANS_SERVICE_UUID))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
