// en: Read/write Age and First Name, notify Database Change Increment on writes
// ja: Age・First Nameのread/write、書き込み時にDatabase Change IncrementをNotify
#include <EspBle.h>

static constexpr const char *USER_DATA_SERVICE_UUID = "181c";
static constexpr const char *AGE_UUID = "2a80";
static constexpr const char *FIRST_NAME_UUID = "2a8a";
static constexpr const char *DB_CHANGE_INCREMENT_UUID = "2a99";

EspBle ble;
EspBleGattService userDataServiceService;
EspBleGattCharacteristic ageCharacteristic;
EspBleGattCharacteristic firstNameCharacteristic;
EspBleGattCharacteristic dbChangeIncrementCharacteristic;
uint32_t dbChangeIncrement = 0;

static void notifyIncrement()
{
  ++dbChangeIncrement;
  uint8_t bytes[4];
  bytes[0] = static_cast<uint8_t>(dbChangeIncrement & 0xFF);
  bytes[1] = static_cast<uint8_t>((dbChangeIncrement >> 8) & 0xFF);
  bytes[2] = static_cast<uint8_t>((dbChangeIncrement >> 16) & 0xFF);
  bytes[3] = static_cast<uint8_t>((dbChangeIncrement >> 24) & 0xFF);
  ble.gattServer().setValue(dbChangeIncrementCharacteristic, bytes, sizeof(bytes));
  // en: Check send acceptance when adapting this example; it is separate from later completion.
  // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
  ble.gattServer().notify(dbChangeIncrementCharacteristic, bytes, sizeof(bytes));
}

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig ageConfig;
  ageConfig.readable = true;
  ageConfig.writable = true;
  EspBleGattCharacteristicConfig nameConfig;
  nameConfig.readable = true;
  nameConfig.writable = true;
  EspBleGattCharacteristicConfig incrementConfig;
  incrementConfig.readable = true;
  incrementConfig.writable = true;
  incrementConfig.notifiable = true;
  auto &server = ble.gattServer();
  const uint8_t initialAge = 25;
  const uint8_t initialIncrement[4] = {0, 0, 0, 0};
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  userDataServiceService = server.addService(USER_DATA_SERVICE_UUID);
  ageCharacteristic = server.addCharacteristic(userDataServiceService, AGE_UUID, ageConfig);
  firstNameCharacteristic = server.addCharacteristic(userDataServiceService, FIRST_NAME_UUID, nameConfig);
  dbChangeIncrementCharacteristic = server.addCharacteristic(userDataServiceService, DB_CHANGE_INCREMENT_UUID, incrementConfig);
  server.setValue(ageCharacteristic, &initialAge, sizeof(initialAge));
  server.setValue(dbChangeIncrementCharacteristic, initialIncrement, sizeof(initialIncrement));

  // en: Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
  // ja: updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
  server.onWritten([](const EspBleGattWrite &write) {
    if (write.characteristicUuid.equalsIgnoreCase(AGE_UUID) && write.value.length() == 1)
    {
      const uint8_t age = static_cast<uint8_t>(write.value[0]);
      ble.gattServer().setValue(ageCharacteristic, &age, sizeof(age));
      Serial.printf("Age updated: %u\n", age);
      notifyIncrement();
    }
    else if (write.characteristicUuid.equalsIgnoreCase(FIRST_NAME_UUID))
    {
      Serial.printf("First Name updated: %s\n", write.value.c_str());
      notifyIncrement();
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle User Data";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(USER_DATA_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
