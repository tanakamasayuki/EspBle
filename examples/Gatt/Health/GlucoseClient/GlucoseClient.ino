// en: RACP report-records request and measurement/response decoding
// ja: RACPレコード要求とMeasurement/応答のdecode
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *GLUCOSE_SERVICE_UUID = "1808";
static constexpr const char *GLUCOSE_MEASUREMENT_UUID = "2a18";
static constexpr const char *RACP_UUID = "2a52";

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
    ble.subscribe(connection.id, GLUCOSE_SERVICE_UUID, GLUCOSE_MEASUREMENT_UUID);
  });
  // en: Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
  // ja: result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(GLUCOSE_MEASUREMENT_UUID) && result.success)
    {
      // en: RACP uses indications (notifications = false).
      // ja: RACPにはIndicateを使います（notifications = false）。
      ble.subscribe(result.connectionId, GLUCOSE_SERVICE_UUID, RACP_UUID, false);
    }
    else if (result.characteristicUuid.equalsIgnoreCase(RACP_UUID) && result.success)
    {
      // en: Report Stored Records (opcode 1), operator All records (1).
      // ja: Report Stored Recordsはopcode 1、全記録を指定するoperatorは1です。
      const uint8_t request[2] = {0x01, 0x01};
      ble.writeCharacteristic(result.connectionId, GLUCOSE_SERVICE_UUID, RACP_UUID, request, sizeof(request), true);
    }
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.characteristicUuid.equalsIgnoreCase(GLUCOSE_MEASUREMENT_UUID) &&
        notification.value.length() >= 13)
    {
      const String &value = notification.value;
      const uint16_t sequence = static_cast<uint8_t>(value[1]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(value[2])) << 8);
      const uint8_t bytes[2] = {static_cast<uint8_t>(value[10]), static_cast<uint8_t>(value[11])};
      Serial.printf("Glucose record #%u: %.0f\n", sequence, espBleReadMedicalSFloatLE(bytes));
    }
    else if (notification.characteristicUuid.equalsIgnoreCase(RACP_UUID) &&
             notification.value.length() >= 4)
    {
      Serial.printf("RACP response: request=%u status=%u\n",
        static_cast<uint8_t>(notification.value[2]), static_cast<uint8_t>(notification.value[3]));
    }
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(GLUCOSE_SERVICE_UUID))
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
