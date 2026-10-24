// en: Feature read and flags-driven Indoor Bike Data (speed/cadence/power) decoding
// ja: Feature Readとflags駆動Indoor Bike Data（speed/cadence/power）のdecode
#include <EspBle.h>

static constexpr const char *FTMS_SERVICE_UUID = "1826";
static constexpr const char *INDOOR_BIKE_DATA_UUID = "2ad2";
static constexpr const char *FITNESS_MACHINE_FEATURE_UUID = "2acc";

EspBle ble;

static uint16_t u16(const String &value, size_t offset)
{
  return static_cast<uint8_t>(value[offset]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
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
    ble.readCharacteristic(connection.id, FTMS_SERVICE_UUID, FITNESS_MACHINE_FEATURE_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 8)
    {
      Serial.println("Fitness Machine Feature read; subscribing to Indoor Bike Data");
      ble.subscribe(result.connectionId, FTMS_SERVICE_UUID, INDOOR_BIKE_DATA_UUID);
    }
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(INDOOR_BIKE_DATA_UUID) ||
        notification.value.length() < 2)
      return;
    const String &value = notification.value;
    const uint16_t flags = u16(value, 0);
    size_t offset = 2;
    uint16_t speed = 0, cadence = 0;
    int16_t power = 0;
    if (!(flags & 0x0001)) { speed = u16(value, offset); offset += 2; } // Instantaneous Speed
    if (flags & 0x0002) offset += 2;                                    // Average Speed
    if (flags & 0x0004) { cadence = u16(value, offset); offset += 2; }  // Instantaneous Cadence
    if (flags & 0x0008) offset += 2;                                    // Average Cadence
    if (flags & 0x0010) offset += 3;                                    // Total Distance
    if (flags & 0x0020) offset += 2;                                    // Resistance Level
    if (flags & 0x0040) { power = static_cast<int16_t>(u16(value, offset)); offset += 2; } // Instantaneous Power
    // en: Speed is 0.01 km/h, cadence is 0.5 /min (so rpm = cadence / 2).
    // ja: 速度は0.01 km/h、ケイデンスは0.5回/分単位です（rpm = cadence / 2）。
    Serial.printf("Speed: %u.%02u km/h  Cadence: %u rpm  Power: %d W\n",
      speed / 100, speed % 100, cadence / 2, power);
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(FTMS_SERVICE_UUID))
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
