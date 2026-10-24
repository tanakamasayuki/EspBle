// en: Write Age/First Name and observe Database Change Increment notifications
// ja: Age/First NameのWriteとDatabase Change IncrementのNotification観測
#include <EspBle.h>

static constexpr const char *USER_DATA_SERVICE_UUID = "181c";
static constexpr const char *AGE_UUID = "2a80";
static constexpr const char *FIRST_NAME_UUID = "2a8a";
static constexpr const char *DB_CHANGE_INCREMENT_UUID = "2a99";

EspBle ble;
EspBleConnectionId connectionId = 0;
bool wroteProfile = false;

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
    wroteProfile = false;
    ble.subscribe(connection.id, USER_DATA_SERVICE_UUID, DB_CHANGE_INCREMENT_UUID);
  });
  // en: Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
  // ja: result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (result.success)
      ble.readCharacteristic(result.connectionId, USER_DATA_SERVICE_UUID, AGE_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(AGE_UUID) && result.success && result.value.length() == 1)
    {
      Serial.printf("Age: %u\n", static_cast<uint8_t>(result.value[0]));
      // en: Write a new profile once, right after reading the current Age.
      // ja: 現在のAgeを読んだ直後に、一度だけ新しいprofileをWriteする。
      if (!wroteProfile)
      {
        wroteProfile = true;
        const uint8_t name[3] = {'A', 'd', 'a'};
        ble.writeCharacteristic(result.connectionId, USER_DATA_SERVICE_UUID, FIRST_NAME_UUID, name, sizeof(name), true);
        const uint8_t age = 42;
        ble.writeCharacteristic(result.connectionId, USER_DATA_SERVICE_UUID, AGE_UUID, &age, sizeof(age), true);
      }
    }
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(DB_CHANGE_INCREMENT_UUID) ||
        notification.value.length() != 4)
      return;
    uint32_t increment = 0;
    for (int i = 3; i >= 0; --i)
      increment = (increment << 8) | static_cast<uint8_t>(notification.value[i]);
    Serial.printf("Database Change Increment: %u\n", static_cast<unsigned>(increment));
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(USER_DATA_SERVICE_UUID))
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
