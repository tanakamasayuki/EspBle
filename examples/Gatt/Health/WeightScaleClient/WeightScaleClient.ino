// en: Feature read and Weight Measurement indication decoding
// ja: Feature ReadとWeight MeasurementのIndication decode
#include <EspBle.h>

static constexpr const char *WEIGHT_SCALE_SERVICE_UUID = "181d";
static constexpr const char *WEIGHT_MEASUREMENT_UUID = "2a9d";
static constexpr const char *WEIGHT_SCALE_FEATURE_UUID = "2a9e";

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
    ble.readCharacteristic(connection.id, WEIGHT_SCALE_SERVICE_UUID, WEIGHT_SCALE_FEATURE_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 4)
    {
      ble.subscribe(result.connectionId, WEIGHT_SCALE_SERVICE_UUID, WEIGHT_MEASUREMENT_UUID, false);
    }
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(WEIGHT_MEASUREMENT_UUID) ||
        notification.value.length() < 3)
      return;
    // en: Flags select units and optional fields; validate the layout before reading further bytes.
    // ja: flagsで単位や任意項目が変わります。後続バイトを読む前に形式を確認します。
    const uint8_t flags = static_cast<uint8_t>(notification.value[0]);
    const uint16_t raw = static_cast<uint8_t>(notification.value[1]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(notification.value[2])) << 8);
    // en: SI weight resolution is 0.005 kg per LSB; Imperial is 0.01 lb.
    // ja: SIの体重分解能は1 LSBあたり0.005 kg、Imperialは0.01 lb。
    const double weight = (flags & 0x01) ? raw * 0.01 : raw * 0.005;
    Serial.printf("Weight: %.3f %s\n", weight, (flags & 0x01) ? "lb" : "kg");
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(WEIGHT_SCALE_SERVICE_UUID))
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
