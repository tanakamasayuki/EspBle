// en: Category bitmask read, Control Point writes, New Alert notifications
// ja: category bitmask Read、Control Point write、New AlertのNotify
#include <EspBle.h>

static constexpr const char *ANS_SERVICE_UUID = "1811";
static constexpr const char *SUPPORTED_NEW_ALERT_CATEGORY_UUID = "2a47";
static constexpr const char *NEW_ALERT_UUID = "2a46";
static constexpr const char *ALERT_CONTROL_POINT_UUID = "2a44";

EspBle ble;
EspBleGattService ansServiceService;
EspBleGattCharacteristic supportedNewAlertCategoryCharacteristic;
EspBleGattCharacteristic newAlertCharacteristic;
EspBleGattCharacteristic alertControlPointCharacteristic;
// en: bit 1 = Email, bit 5 = SMS/MMS / ja: bit 1 = Email、bit 5 = SMS/MMS
const uint8_t supportedCategories[2] = {0x22, 0x00};

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig categoryConfig;
  categoryConfig.readable = true;
  EspBleGattCharacteristicConfig alertConfig;
  alertConfig.notifiable = true;
  EspBleGattCharacteristicConfig controlConfig;
  controlConfig.writable = true;
  auto &server = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  ansServiceService = server.addService(ANS_SERVICE_UUID);
  supportedNewAlertCategoryCharacteristic = server.addCharacteristic(ansServiceService, SUPPORTED_NEW_ALERT_CATEGORY_UUID, categoryConfig);
  newAlertCharacteristic = server.addCharacteristic(ansServiceService, NEW_ALERT_UUID, alertConfig);
  alertControlPointCharacteristic = server.addCharacteristic(ansServiceService, ALERT_CONTROL_POINT_UUID, controlConfig);
  server.setValue(supportedNewAlertCategoryCharacteristic, supportedCategories, sizeof(supportedCategories));

  // en: Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
  // ja: updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(ALERT_CONTROL_POINT_UUID) || write.value.length() < 2)
      return;
    const uint8_t command = static_cast<uint8_t>(write.value[0]);
    const uint8_t category = static_cast<uint8_t>(write.value[1]);
    // en: Command 2 = Notify New Alert Immediately / ja: command 2 = Notify New Alert Immediately
    if (command == 0x02)
    {
      Serial.printf("Notify New Alert for category %u\n", category);
      const uint8_t alert[5] = {category, 3, 'B', 'o', 'b'};
      // en: Check send acceptance when adapting this example; it is separate from later completion.
      // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
      ble.gattServer().notify(newAlertCharacteristic, alert, sizeof(alert));
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Alert Notification";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(ANS_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
