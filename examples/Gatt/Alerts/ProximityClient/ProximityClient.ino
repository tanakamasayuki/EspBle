// en: Proximity Monitor: read Tx Power, arm Link Loss Alert Level
// ja: Proximity Monitor: Tx Power ReadとLink Loss Alert Levelのarm
#include <EspBle.h>

static constexpr const char *LINK_LOSS_SERVICE_UUID = "1803";
static constexpr const char *ALERT_LEVEL_UUID = "2a06";
static constexpr const char *TX_POWER_SERVICE_UUID = "1804";
static constexpr const char *TX_POWER_LEVEL_UUID = "2a07";

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
    ble.readCharacteristic(connection.id, TX_POWER_SERVICE_UUID, TX_POWER_LEVEL_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(TX_POWER_LEVEL_UUID) &&
        result.success && result.value.length() == 1)
    {
      Serial.printf("Tx Power Level: %d dBm\n", static_cast<int8_t>(result.value[0]));
      ble.readCharacteristic(result.connectionId, LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID);
    }
    else if (result.characteristicUuid.equalsIgnoreCase(ALERT_LEVEL_UUID) &&
             result.success && result.value.length() == 1)
    {
      Serial.printf("Link Loss Alert Level: %u\n", static_cast<uint8_t>(result.value[0]));
      // en: Arm High Alert (2) so the Reporter alerts on link loss.
      // ja: High Alert（2）を設定し、link loss時にReporterが鳴動するようにする。
      const uint8_t level = 2;
      ble.writeCharacteristic(result.connectionId, LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID, &level, sizeof(level), true);
    }
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(ALERT_LEVEL_UUID) && result.success)
      Serial.println("Armed Link Loss High Alert");
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(LINK_LOSS_SERVICE_UUID))
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
