// en: Alert Status / Ringer Setting notify, Ringer Control Point silent-mode
// ja: Alert Status / Ringer SettingのNotify、Ringer Control PointでSilent Mode
#include <EspBle.h>

static constexpr const char *PASS_SERVICE_UUID = "180e";
static constexpr const char *ALERT_STATUS_UUID = "2a3f";
static constexpr const char *RINGER_SETTING_UUID = "2a41";
static constexpr const char *RINGER_CONTROL_POINT_UUID = "2a40";

EspBle ble;
EspBleGattService passServiceService;
EspBleGattCharacteristic alertStatusCharacteristic;
EspBleGattCharacteristic ringerSettingCharacteristic;
EspBleGattCharacteristic ringerControlPointCharacteristic;
// en: bit 0 = Ringer State active
// ja: bit 0 = Ringer State稼働
const uint8_t alertStatus = 0x01;
// en: 0 = Silent, 1 = Normal
// ja: 0 = Silent、1 = Normal
uint8_t ringerSetting = 1;

static void setRingerSetting(uint8_t value)
{
  ringerSetting = value;
  ble.gattServer().setValue(ringerSettingCharacteristic, &ringerSetting, sizeof(ringerSetting));
  // en: Check send acceptance when adapting this example; it is separate from later completion.
  // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
  ble.gattServer().notify(ringerSettingCharacteristic, &ringerSetting, sizeof(ringerSetting));
}

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig alertStatusConfig;
  alertStatusConfig.readable = true;
  alertStatusConfig.notifiable = true;
  EspBleGattCharacteristicConfig ringerSettingConfig;
  ringerSettingConfig.readable = true;
  ringerSettingConfig.notifiable = true;
  EspBleGattCharacteristicConfig controlConfig;
  controlConfig.writableWithoutResponse = true;
  auto &server = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  passServiceService = server.addService(PASS_SERVICE_UUID);
  alertStatusCharacteristic = server.addCharacteristic(passServiceService, ALERT_STATUS_UUID, alertStatusConfig);
  ringerSettingCharacteristic = server.addCharacteristic(passServiceService, RINGER_SETTING_UUID, ringerSettingConfig);
  ringerControlPointCharacteristic = server.addCharacteristic(passServiceService, RINGER_CONTROL_POINT_UUID, controlConfig);
  server.setValue(alertStatusCharacteristic, &alertStatus, sizeof(alertStatus));
  server.setValue(ringerSettingCharacteristic, &ringerSetting, sizeof(ringerSetting));

  // en: Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
  // ja: updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(RINGER_CONTROL_POINT_UUID) || write.value.length() != 1)
      return;
    const uint8_t command = static_cast<uint8_t>(write.value[0]);
    // en: 1 = Set Silent Mode, 3 = Cancel Silent Mode / ja: 1 = Set Silent、3 = Cancel Silent
    if (command == 1)
    {
      Serial.println("Silent Mode");
      setRingerSetting(0);
    }
    else if (command == 3)
    {
      Serial.println("Normal Mode");
      setRingerSetting(1);
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Phone Alert";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(PASS_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
