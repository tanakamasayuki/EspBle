// en: Time Update Control Point drives a readable Time Update State
// ja: Time Update Control Pointがread可能なTime Update Stateを駆動
#include <EspBle.h>

static constexpr const char *RTUS_SERVICE_UUID = "1806";
static constexpr const char *TIME_UPDATE_CONTROL_POINT_UUID = "2a16";
static constexpr const char *TIME_UPDATE_STATE_UUID = "2a17";

EspBle ble;
EspBleGattService rtusServiceService;
EspBleGattCharacteristic timeUpdateControlPointCharacteristic;
EspBleGattCharacteristic timeUpdateStateCharacteristic;
// en: Current State: 0 = Idle, 1 = Update Pending. Result: 0 = Successful, 1 = Canceled.
// ja: Current State: 0 = Idle、1 = Update Pending。Result: 0 = Successful、1 = Canceled。
uint8_t timeUpdateState[2] = {0, 0};

static void setState(uint8_t current, uint8_t resultCode)
{
  timeUpdateState[0] = current;
  timeUpdateState[1] = resultCode;
  ble.gattServer().setValue(timeUpdateStateCharacteristic, timeUpdateState, sizeof(timeUpdateState));
}

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig controlConfig;
  controlConfig.writableWithoutResponse = true;
  EspBleGattCharacteristicConfig stateConfig;
  stateConfig.readable = true;
  auto &server = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  rtusServiceService = server.addService(RTUS_SERVICE_UUID);
  timeUpdateControlPointCharacteristic = server.addCharacteristic(rtusServiceService, TIME_UPDATE_CONTROL_POINT_UUID, controlConfig);
  timeUpdateStateCharacteristic = server.addCharacteristic(rtusServiceService, TIME_UPDATE_STATE_UUID, stateConfig);
  server.setValue(timeUpdateStateCharacteristic, timeUpdateState, sizeof(timeUpdateState));

  // en: Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
  // ja: updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(TIME_UPDATE_CONTROL_POINT_UUID) || write.value.length() != 1)
      return;
    const uint8_t command = static_cast<uint8_t>(write.value[0]);
    if (command == 1)
    {
      Serial.println("Get Reference Update");
      setState(1, 0); // en: Update Pending / ja: Update Pending
    }
    else if (command == 2)
    {
      Serial.println("Cancel Reference Update");
      setState(0, 1); // en: Idle, Canceled / ja: Idle、Canceled
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Time Update";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(RTUS_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
