// en: Features read and SpO2/pulse-rate measurement decoding
// ja: Features ReadとSpO2/pulse-rate測定値のdecode
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *PLX_SERVICE_UUID = "1822";
static constexpr const char *PLX_SPOT_CHECK_UUID = "2a5e";
static constexpr const char *PLX_FEATURES_UUID = "2a60";

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
    ble.readCharacteristic(connection.id, PLX_SERVICE_UUID, PLX_FEATURES_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() >= 2)
    {
      // en: PLX Spot-Check Measurement is an indication (notifications = false).
      // ja: PLX Spot-Check MeasurementはIndicateです（notifications = false）。
      ble.subscribe(result.connectionId, PLX_SERVICE_UUID, PLX_SPOT_CHECK_UUID, false);
    }
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(PLX_SPOT_CHECK_UUID) ||
        notification.value.length() < 5)
      return;
    const String &value = notification.value;
    const uint8_t spo2Bytes[2] = {static_cast<uint8_t>(value[1]), static_cast<uint8_t>(value[2])};
    const uint8_t prBytes[2] = {static_cast<uint8_t>(value[3]), static_cast<uint8_t>(value[4])};
    Serial.printf("SpO2: %.0f %%, pulse: %.0f bpm\n",
      espBleReadMedicalSFloatLE(spo2Bytes), espBleReadMedicalSFloatLE(prBytes));
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(PLX_SERVICE_UUID))
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
