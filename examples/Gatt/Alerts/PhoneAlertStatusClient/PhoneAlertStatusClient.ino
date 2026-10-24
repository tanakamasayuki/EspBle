// en: Read Alert Status, drive Ringer Control Point, decode Ringer Setting
// ja: Alert Status Read、Ringer Control Point操作、Ringer Settingのdecode
#include <EspBle.h>

static constexpr const char *PASS_SERVICE_UUID = "180e";
static constexpr const char *ALERT_STATUS_UUID = "2a3f";
static constexpr const char *RINGER_SETTING_UUID = "2a41";
static constexpr const char *RINGER_CONTROL_POINT_UUID = "2a40";

EspBle ble;
EspBleConnectionId connectionId = 0;
unsigned long lastToggle = 0;
// en: 1 = Silent, 3 = Cancel
// ja: 1 = Silent、3 = Cancel
uint8_t nextCommand = 1;

static void writeRingerControl(uint8_t command)
{
  // en: Ringer Control Point is Write Without Response.
  // ja: Ringer Control PointはWrite Without Response。
  ble.writeCharacteristic(
    connectionId, PASS_SERVICE_UUID, RINGER_CONTROL_POINT_UUID, &command, sizeof(command), false);
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
    connectionId = connection.id;
    ble.readCharacteristic(connection.id, PASS_SERVICE_UUID, ALERT_STATUS_UUID);
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(ALERT_STATUS_UUID) &&
        result.success && result.value.length() == 1)
    {
      Serial.printf("Alert Status: 0x%02x\n", static_cast<uint8_t>(result.value[0]));
      ble.subscribe(result.connectionId, PASS_SERVICE_UUID, RINGER_SETTING_UUID);
    }
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(RINGER_SETTING_UUID) ||
        notification.value.length() != 1)
      return;
    const uint8_t setting = static_cast<uint8_t>(notification.value[0]);
    Serial.printf("Ringer Setting: %s\n", setting == 0 ? "Silent" : "Normal");
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(PASS_SERVICE_UUID))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  // en: Every 5 seconds, toggle between Set Silent Mode and Cancel Silent Mode.
  // ja: 5秒ごとに、Set Silent ModeとCancel Silent Modeを切り替える。
  if (connectionId != 0 && millis() - lastToggle >= 5000)
  {
    lastToggle = millis();
    writeRingerControl(nextCommand);
    nextCommand = (nextCommand == 1) ? 3 : 1;
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
