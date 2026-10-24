// en: Find Me target: Alert Level Write Without Response handling
// ja: Find Meターゲット: Alert LevelのWrite Without Response処理
#include <EspBle.h>

static constexpr const char *IMMEDIATE_ALERT_SERVICE_UUID = "1802";
static constexpr const char *ALERT_LEVEL_UUID = "2a06";

EspBle ble;

EspBleGattService immediateAlertServiceService;
EspBleGattCharacteristic alertLevelCharacteristic;
void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig alertLevelConfig;
  alertLevelConfig.writable = true;
  alertLevelConfig.writableWithoutResponse = true;
  auto &server = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  immediateAlertServiceService = server.addService(IMMEDIATE_ALERT_SERVICE_UUID);
  alertLevelCharacteristic = server.addCharacteristic(immediateAlertServiceService, ALERT_LEVEL_UUID, alertLevelConfig);

  // en: Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
  // ja: updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(ALERT_LEVEL_UUID) || write.value.length() != 1)
      return;
    const uint8_t level = static_cast<uint8_t>(write.value[0]);
    // en: A real device would beep/vibrate here. / ja: 実機ならここで鳴動・振動する。
    const char *name = level == 0 ? "No Alert" : (level == 1 ? "Mild Alert" : "High Alert");
    Serial.printf("Alert Level: %u (%s)\n", level, name);
  });

  EspBleConfig config;
  config.deviceName = "EspBle Find Me";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(IMMEDIATE_ALERT_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
