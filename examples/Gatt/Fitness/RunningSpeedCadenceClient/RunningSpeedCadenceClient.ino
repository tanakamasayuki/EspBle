// en: Sensor Location read and RSC Measurement notification decoding
// ja: Sensor Location ReadとRSC MeasurementのNotification decode
#include <EspBle.h>

static constexpr const char *RSC_SERVICE_UUID = "1814";
static constexpr const char *RSC_MEASUREMENT_UUID = "2a53";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

EspBle ble;

static uint16_t readU16(const String &value, size_t offset)
{
  return static_cast<uint8_t>(value[offset]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
}

static uint32_t readU32(const String &value, size_t offset)
{
  return static_cast<uint32_t>(static_cast<uint8_t>(value[offset])) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 1])) << 8) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 2])) << 16) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 3])) << 24);
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
    ble.readCharacteristic(connection.id, RSC_SERVICE_UUID, SENSOR_LOCATION_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 1)
    {
      Serial.printf("Sensor location: %u\n", static_cast<uint8_t>(result.value[0]));
      ble.subscribe(result.connectionId, RSC_SERVICE_UUID, RSC_MEASUREMENT_UUID);
    }
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(RSC_MEASUREMENT_UUID) ||
        notification.value.length() < 4)
      return;
    const String &value = notification.value;
    // en: Flags select units and optional fields; validate the layout before reading further bytes.
    // ja: flagsで単位や任意項目が変わります。後続バイトを読む前に形式を確認します。
    const uint8_t flags = static_cast<uint8_t>(value[0]);
    const double speed = readU16(value, 1) / 256.0; // m/s
    const uint8_t cadence = static_cast<uint8_t>(value[3]);
    Serial.printf("%s: %.2f m/s, cadence %u /min", (flags & 0x04) ? "Running" : "Walking", speed, cadence);
    size_t offset = 4;
    if ((flags & 0x01) && value.length() >= offset + 2)
    {
      Serial.printf(", stride %.2f m", readU16(value, offset) / 100.0);
      offset += 2;
    }
    if ((flags & 0x02) && value.length() >= offset + 4)
    {
      Serial.printf(", distance %.1f m", readU32(value, offset) / 10.0);
    }
    Serial.println();
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(RSC_SERVICE_UUID))
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
