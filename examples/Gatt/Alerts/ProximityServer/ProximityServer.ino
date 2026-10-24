// en: Proximity Reporter: Link Loss Alert Level + Tx Power (two services)
// ja: Proximity Reporter: Link Loss Alert Level＋Tx Power（2 Service）
#include <EspBle.h>

static constexpr const char *LINK_LOSS_SERVICE_UUID = "1803";
static constexpr const char *ALERT_LEVEL_UUID = "2a06";
static constexpr const char *TX_POWER_SERVICE_UUID = "1804";
static constexpr const char *TX_POWER_LEVEL_UUID = "2a07";

EspBle ble;
EspBleGattService linkLossServiceService;
EspBleGattService txPowerServiceService;
EspBleGattCharacteristic alertLevelCharacteristic;
EspBleGattCharacteristic txPowerLevelCharacteristic;
const int8_t txPowerLevel = -8; // en: dBm / ja: dBm

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig alertLevelConfig;
  alertLevelConfig.readable = true;
  alertLevelConfig.writable = true;
  EspBleGattCharacteristicConfig txPowerConfig;
  txPowerConfig.readable = true;
  auto &server = ble.gattServer();
  const uint8_t initialAlert = 0; // en: No Alert / ja: No Alert
  const uint8_t txPowerByte = static_cast<uint8_t>(txPowerLevel);
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  linkLossServiceService = server.addService(LINK_LOSS_SERVICE_UUID);
  alertLevelCharacteristic = server.addCharacteristic(linkLossServiceService, ALERT_LEVEL_UUID, alertLevelConfig);
  txPowerServiceService = server.addService(TX_POWER_SERVICE_UUID);
  txPowerLevelCharacteristic = server.addCharacteristic(txPowerServiceService, TX_POWER_LEVEL_UUID, txPowerConfig);
  server.setValue(alertLevelCharacteristic, &initialAlert, sizeof(initialAlert));
  server.setValue(txPowerLevelCharacteristic, &txPowerByte, sizeof(txPowerByte));

  // en: Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
  // ja: updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(ALERT_LEVEL_UUID) || write.value.length() != 1)
      return;
    const uint8_t level = static_cast<uint8_t>(write.value[0]);
    ble.gattServer().setValue(alertLevelCharacteristic, &level, sizeof(level));
    const char *name = level == 0 ? "No Alert" : (level == 1 ? "Mild Alert" : "High Alert");
    Serial.printf("Link Loss Alert Level: %u (%s)\n", level, name);
  });

  EspBleConfig config;
  config.deviceName = "EspBle Proximity";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(LINK_LOSS_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
