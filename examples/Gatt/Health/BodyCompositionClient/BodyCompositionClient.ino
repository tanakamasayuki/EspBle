// en: Feature read and Body Fat Percentage / Weight measurement decoding
// ja: Feature ReadとBody Fat Percentage / Weight測定値のdecode
#include <EspBle.h>

static constexpr const char *BODY_COMPOSITION_SERVICE_UUID = "181b";
static constexpr const char *BODY_COMPOSITION_MEASUREMENT_UUID = "2a9c";
static constexpr const char *BODY_COMPOSITION_FEATURE_UUID = "2a9b";

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
    ble.readCharacteristic(connection.id, BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_FEATURE_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 4)
    {
      ble.subscribe(result.connectionId, BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_MEASUREMENT_UUID, false);
    }
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(BODY_COMPOSITION_MEASUREMENT_UUID) ||
        notification.value.length() < 4)
      return;
    const uint16_t flags = static_cast<uint8_t>(notification.value[0]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(notification.value[1])) << 8);
    const uint16_t fatRaw = static_cast<uint8_t>(notification.value[2]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(notification.value[3])) << 8);
    // en: Body Fat Percentage resolution is 0.1 % per LSB.
    // ja: Body Fat Percentageの分解能は1 LSBあたり0.1 %。
    Serial.printf("Body fat: %.1f %%", fatRaw * 0.1);
    // en: Weight (flag bit 10) is the only optional field this example emits, so
    //     it follows the mandatory Body Fat Percentage directly when present.
    // ja: この例が出す任意フィールドはWeight（flag bit 10）だけなので、存在すれば
    //     必須のBody Fat Percentageの直後に続く。
    if ((flags & 0x0400) && notification.value.length() >= 6)
    {
      const uint16_t weightRaw = static_cast<uint8_t>(notification.value[4]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(notification.value[5])) << 8);
      const double weight = (flags & 0x01) ? weightRaw * 0.01 : weightRaw * 0.005;
      Serial.printf(", weight: %.3f %s", weight, (flags & 0x01) ? "lb" : "kg");
    }
    Serial.println();
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(BODY_COMPOSITION_SERVICE_UUID))
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
