// en: Standard 10-byte Current Time reads and notifications
// ja: 標準10-byte Current TimeのReadとNotification
#include <EspBle.h>

static constexpr const char *CURRENT_TIME_SERVICE_UUID = "1805";
static constexpr const char *CURRENT_TIME_UUID = "2a2b";

EspBle ble;
EspBleGattService currentTimeServiceService;
EspBleGattCharacteristic currentTimeCharacteristic;
// en: 2026-07-19 12:34:56, Sunday, 0/256 second, manually adjusted.
// ja: 固定のデモ時刻は2026-07-19 12:34:56、日曜、小数秒0、手動変更です。
uint8_t currentTime[] = {0xea, 0x07, 7, 19, 12, 34, 56, 7, 0, 0x01};

static void publishCurrentTime()
{
  auto &server = ble.gattServer();
  server.setValue(currentTimeCharacteristic, currentTime, sizeof(currentTime));
  // en: Check send acceptance when adapting this example; it is separate from later completion.
  // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
  const bool notified = server.notify(currentTimeCharacteristic, currentTime, sizeof(currentTime));
  Serial.printf("Time: %04u-%02u-%02u %02u:%02u:%02u (notification accepted: %u)\n",
    static_cast<unsigned>(currentTime[0] | (currentTime[1] << 8)),
    currentTime[2], currentTime[3], currentTime[4], currentTime[5], currentTime[6],
    notified ? 1 : 0);
}

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig timeConfig;
  timeConfig.readable = true;
  timeConfig.notifiable = true;
  auto &server = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  if (!(currentTimeServiceService = server.addService(CURRENT_TIME_SERVICE_UUID)).valid() ||
      !(currentTimeCharacteristic = server.addCharacteristic(currentTimeServiceService, CURRENT_TIME_UUID, timeConfig)).valid() ||
      !server.setValue(currentTimeCharacteristic, currentTime, sizeof(currentTime)))
  {
    Serial.printf("Current Time configuration failed: %s\n",
      ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle Current Time";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Current Time");
  ble.advertising().addServiceUuid(CURRENT_TIME_SERVICE_UUID);
  ble.advertising().start();
  Serial.println("Send 't' to advance one second and notify subscribers.");
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == 't')
  {
    currentTime[6] = static_cast<uint8_t>((currentTime[6] + 1) % 60);
    publishCurrentTime();
  }
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
