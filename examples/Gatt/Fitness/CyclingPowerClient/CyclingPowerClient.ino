// en: Sensor Location read and signed power measurement decoding
// ja: Sensor Location Readと符号付きpower測定値のdecode
#include <EspBle.h>

static constexpr const char *CYCLING_POWER_SERVICE_UUID = "1818";
static constexpr const char *CYCLING_POWER_MEASUREMENT_UUID = "2a63";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

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
    ble.readCharacteristic(connection.id, CYCLING_POWER_SERVICE_UUID, SENSOR_LOCATION_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 1)
    {
      Serial.printf("Sensor location: %u\n", static_cast<uint8_t>(result.value[0]));
      ble.subscribe(result.connectionId, CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_MEASUREMENT_UUID);
    }
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(CYCLING_POWER_MEASUREMENT_UUID) ||
        notification.value.length() < 4)
      return;
    const String &value = notification.value;
    const uint16_t rawPower = static_cast<uint8_t>(value[2]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(value[3])) << 8);
    // en: Instantaneous power is a signed 16-bit value in watts.
    // ja: 瞬時パワーはW単位の符号付き16ビット値です。
    Serial.printf("Power: %d W\n", static_cast<int16_t>(rawPower));
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(CYCLING_POWER_SERVICE_UUID))
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
