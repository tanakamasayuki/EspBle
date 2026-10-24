// en: Scaled sensor reads and Temperature notification subscription
// ja: scale付きSensor値ReadとTemperature Notification購読
#include <EspBle.h>

static constexpr const char *ENVIRONMENTAL_SENSING_SERVICE_UUID = "181a";
static constexpr const char *CHARACTERISTIC_UUIDS[] = {"2a6e", "2a6f", "2a6d"};
static constexpr const char *TEMPERATURE_UUID = "2a6e";

EspBle ble;
bool connectionRequested = false;
size_t readIndex = 0;

static uint16_t decode16(const String &value)
{
  return static_cast<uint8_t>(value[0]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[1])) << 8);
}

static uint32_t decode32(const String &value)
{
  return static_cast<uint8_t>(value[0]) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[1])) << 8) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[2])) << 16) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[3])) << 24);
}

static void printTemperature(const String &value, const char *label)
{
  if (value.length() != 2)
  {
    Serial.printf("%s has invalid length\n", label);
    return;
  }
  Serial.printf("%s raw: %d (0.01 C units)\n",
    label, static_cast<int16_t>(decode16(value)));
}

static bool requestRead(EspBleConnectionId connectionId)
{
  return ble.readCharacteristic(connectionId, ENVIRONMENTAL_SENSING_SERVICE_UUID,
    CHARACTERISTIC_UUIDS[readIndex]);
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
    readIndex = 0;
    requestRead(connection.id);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("Environmental read failed: %s\n", result.detail.c_str());
      return;
    }
    if (readIndex == 0)
    {
      printTemperature(result.value, "Temperature");
    }
    else if (readIndex == 1 && result.value.length() == 2)
    {
      Serial.printf("Humidity raw: %u (0.01 %% units)\n", decode16(result.value));
    }
    else if (readIndex == 2 && result.value.length() == 4)
    {
      Serial.printf("Pressure raw: %lu (0.1 Pa units)\n",
        static_cast<unsigned long>(decode32(result.value)));
    }
    else
    {
      Serial.println("Environmental value has invalid length");
      return;
    }

    ++readIndex;
    if (readIndex < 3)
    {
      requestRead(result.connectionId);
    }
    else
    {
      ble.subscribe(result.connectionId, ENVIRONMENTAL_SENSING_SERVICE_UUID,
        TEMPERATURE_UUID);
    }
  });
  // en: Set application readiness only after result.success confirms the subscription. Link connection by itself is not a subscription.
  // ja: result.successで購読成功を確認してから、アプリの準備完了とします。接続成立だけでは購読できていません。
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("Temperature subscription: %s\n", result.success ? "ready" : "failed");
  });
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.serviceUuid.equalsIgnoreCase(ENVIRONMENTAL_SENSING_SERVICE_UUID) &&
        notification.characteristicUuid.equalsIgnoreCase(TEMPERATURE_UUID))
    {
      printTemperature(notification.value, "Temperature changed");
    }
  });
  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested ||
        !result.advertisesService(ENVIRONMENTAL_SENSING_SERVICE_UUID)) return;
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
