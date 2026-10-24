// en: Temperature Type read and FLOAT measurement indication decoding
// ja: Temperature Type ReadとFLOAT測定値のIndication decode
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *HEALTH_THERMOMETER_SERVICE_UUID = "1809";
static constexpr const char *TEMPERATURE_MEASUREMENT_UUID = "2a1c";
static constexpr const char *TEMPERATURE_TYPE_UUID = "2a1d";

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
    ble.readCharacteristic(connection.id, HEALTH_THERMOMETER_SERVICE_UUID, TEMPERATURE_TYPE_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 1)
    {
      Serial.printf("Temperature type: %u\n", static_cast<uint8_t>(result.value[0]));
      // en: Temperature Measurement is an indication (notifications = false).
      // ja: Temperature MeasurementはIndication（notifications = false）。
      ble.subscribe(result.connectionId, HEALTH_THERMOMETER_SERVICE_UUID, TEMPERATURE_MEASUREMENT_UUID, false);
    }
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(TEMPERATURE_MEASUREMENT_UUID) ||
        notification.value.length() < 5)
      return;
    const uint8_t bytes[4] = {
      static_cast<uint8_t>(notification.value[1]),
      static_cast<uint8_t>(notification.value[2]),
      static_cast<uint8_t>(notification.value[3]),
      static_cast<uint8_t>(notification.value[4])};
    const double temperature = espBleReadMedicalFloat32LE(bytes);
    // en: Flags select units and optional fields; validate the layout before reading further bytes.
    // ja: flagsで単位や任意項目が変わります。後続バイトを読む前に形式を確認します。
    const uint8_t flags = static_cast<uint8_t>(notification.value[0]);
    Serial.printf("Temperature: %.2f %s\n", temperature, (flags & 0x01) ? "F" : "C");
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(HEALTH_THERMOMETER_SERVICE_UUID))
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
