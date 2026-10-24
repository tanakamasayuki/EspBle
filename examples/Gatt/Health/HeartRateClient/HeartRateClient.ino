// en: Flags-driven Heart Rate Measurement decoding and subscription
// ja: flagsに従うHeart Rate Measurementのdecodeと購読
#include <EspBle.h>

static constexpr const char *HEART_RATE_SERVICE_UUID = "180d";
static constexpr const char *HEART_RATE_MEASUREMENT_UUID = "2a37";
static constexpr const char *BODY_SENSOR_LOCATION_UUID = "2a38";

EspBle ble;
bool connectionRequested = false;

static bool printMeasurement(const String &value)
{
  if (value.length() < 2) return false;
  // en: Flags select units and optional fields; validate the layout before reading further bytes.
  // ja: flagsで単位や任意項目が変わります。後続バイトを読む前に形式を確認します。
  const uint8_t flags = static_cast<uint8_t>(value[0]);
  size_t offset = 1;
  uint16_t heartRate = 0;
  if ((flags & 0x01) != 0)
  {
    if (offset + 2 > value.length()) return false;
    heartRate = static_cast<uint8_t>(value[offset]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
    offset += 2;
  }
  else
  {
    heartRate = static_cast<uint8_t>(value[offset++]);
  }

  bool hasEnergy = (flags & 0x08) != 0;
  uint16_t energy = 0;
  if (hasEnergy)
  {
    if (offset + 2 > value.length()) return false;
    energy = static_cast<uint8_t>(value[offset]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
    offset += 2;
  }
  const size_t rrCount = (flags & 0x10) != 0 ? (value.length() - offset) / 2 : 0;
  if ((flags & 0x10) != 0 && ((value.length() - offset) % 2) != 0) return false;
  if ((flags & 0x10) == 0 && offset != value.length()) return false;

  Serial.printf("Heart Rate: %u bpm", heartRate);
  if (hasEnergy) Serial.printf(", energy: %u kJ", energy);
  if (rrCount > 0)
  {
    const uint16_t rr = static_cast<uint8_t>(value[offset]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
    Serial.printf(", RR intervals: %u (first: %u/1024 s)",
      static_cast<unsigned>(rrCount), rr);
  }
  Serial.println();
  return true;
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
    ble.readCharacteristic(
      connection.id, HEART_RATE_SERVICE_UUID, BODY_SENSOR_LOCATION_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.success || result.value.length() != 1)
    {
      Serial.printf("Body Sensor Location read failed: %s\n", result.detail.c_str());
      return;
    }
    Serial.printf("Body Sensor Location: %u\n", static_cast<uint8_t>(result.value[0]));
    ble.subscribe(
      result.connectionId, HEART_RATE_SERVICE_UUID, HEART_RATE_MEASUREMENT_UUID);
  });
  // en: Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
  // ja: result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("Heart Rate subscription: %s\n", result.success ? "ready" : "failed");
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.serviceUuid.equalsIgnoreCase(HEART_RATE_SERVICE_UUID) &&
        notification.characteristicUuid.equalsIgnoreCase(HEART_RATE_MEASUREMENT_UUID) &&
        !printMeasurement(notification.value))
    {
      Serial.println("Invalid Heart Rate Measurement");
    }
  });
  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(HEART_RATE_SERVICE_UUID)) return;
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
