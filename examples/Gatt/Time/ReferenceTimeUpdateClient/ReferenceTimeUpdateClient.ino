// en: Request/cancel a reference update and read the Time Update State
// ja: reference update要求/キャンセルとTime Update StateのRead
#include <EspBle.h>

static constexpr const char *RTUS_SERVICE_UUID = "1806";
static constexpr const char *TIME_UPDATE_CONTROL_POINT_UUID = "2a16";
static constexpr const char *TIME_UPDATE_STATE_UUID = "2a17";

EspBle ble;
EspBleConnectionId connectionId = 0;
unsigned long lastAction = 0;
int step = 0;

static void writeControlPoint(uint8_t command)
{
  // en: Write Without Response. / ja: Write Without Response。
  ble.writeCharacteristic(
    connectionId, RTUS_SERVICE_UUID, TIME_UPDATE_CONTROL_POINT_UUID, &command, sizeof(command), false);
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
    lastAction = millis();
    step = 0;
    ble.readCharacteristic(connection.id, RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID);
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(TIME_UPDATE_STATE_UUID) &&
        result.success && result.value.length() == 2)
    {
      const uint8_t current = static_cast<uint8_t>(result.value[0]);
      const uint8_t resultCode = static_cast<uint8_t>(result.value[1]);
      Serial.printf("Time Update State: current=%u result=%u\n", current, resultCode);
    }
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(RTUS_SERVICE_UUID))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  // en: Walk through request -> read -> cancel -> read, two seconds apart.
  // ja: 2秒間隔で 要求 → read → キャンセル → read を進める。
  if (connectionId != 0 && millis() - lastAction >= 2000)
  {
    lastAction = millis();
    if (step == 0)
      // en: Get Reference Update
      // ja: Get Reference Update
      writeControlPoint(1);
    else if (step == 1)
      ble.readCharacteristic(connectionId, RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID);
    else if (step == 2)
      // en: Cancel Reference Update
      // ja: Cancel Reference Update
      writeControlPoint(2);
    else if (step == 3)
      ble.readCharacteristic(connectionId, RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID);
    if (step <= 3)
      ++step;
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
