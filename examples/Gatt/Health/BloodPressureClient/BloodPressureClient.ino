// en: Feature read and SFLOAT measurement indication decoding
// ja: Feature ReadとSFLOAT測定値のIndication decode
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *BLOOD_PRESSURE_SERVICE_UUID = "1810";
static constexpr const char *BLOOD_PRESSURE_MEASUREMENT_UUID = "2a35";
static constexpr const char *BLOOD_PRESSURE_FEATURE_UUID = "2a49";

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
    ble.readCharacteristic(connection.id, BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_FEATURE_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 2)
    {
      // en: Blood Pressure Measurement is an indication (notifications = false).
      // ja: Blood Pressure MeasurementはIndication（notifications = false）。
      ble.subscribe(result.connectionId, BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_MEASUREMENT_UUID, false);
    }
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(BLOOD_PRESSURE_MEASUREMENT_UUID) ||
        notification.value.length() < 7)
      return;
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(notification.value.c_str());
    const double systolic = espBleReadMedicalSFloatLE(&bytes[1]);
    const double diastolic = espBleReadMedicalSFloatLE(&bytes[3]);
    const double mean = espBleReadMedicalSFloatLE(&bytes[5]);
    Serial.printf("Blood pressure: %.0f/%.0f mmHg (mean %.0f)\n", systolic, diastolic, mean);
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(BLOOD_PRESSURE_SERVICE_UUID))
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
