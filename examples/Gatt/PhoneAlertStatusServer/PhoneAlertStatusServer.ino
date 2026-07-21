// en: PhoneAlertStatusServer - standard Phone Alert Status Service (0x180E).
//     Alert Status (0x2A3F) and Ringer Setting (0x2A41) are read/notify uint8
//     values; the Ringer Control Point (0x2A40) is Write Without Response. Set
//     Silent Mode (1) switches Ringer Setting to Silent and notifies; Cancel
//     Silent Mode (3) switches it back to Normal.
// ja: PhoneAlertStatusServer - 標準Phone Alert Status Service（0x180E）。Alert
//     Status（0x2A3F）とRinger Setting（0x2A41）はread/notifyのuint8、Ringer
//     Control Point（0x2A40）はWrite Without Response。Set Silent Mode（1）で
//     Ringer SettingをSilentにしてNotify、Cancel Silent Mode（3）でNormalに戻す。
#include <EspBle.h>

static constexpr const char *PASS_SERVICE_UUID = "180e";
static constexpr const char *ALERT_STATUS_UUID = "2a3f";
static constexpr const char *RINGER_SETTING_UUID = "2a41";
static constexpr const char *RINGER_CONTROL_POINT_UUID = "2a40";

EspBle ble;
const uint8_t alertStatus = 0x01; // en: bit 0 = Ringer State active / ja: bit 0 = Ringer State稼働
uint8_t ringerSetting = 1;        // en: 0 = Silent, 1 = Normal / ja: 0 = Silent、1 = Normal

static void setRingerSetting(uint8_t value)
{
  ringerSetting = value;
  ble.gattServer().setValue(PASS_SERVICE_UUID, RINGER_SETTING_UUID, &ringerSetting, sizeof(ringerSetting));
  ble.gattServer().notify(PASS_SERVICE_UUID, RINGER_SETTING_UUID, &ringerSetting, sizeof(ringerSetting));
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
  server.addService(PASS_SERVICE_UUID);
  server.addCharacteristic(PASS_SERVICE_UUID, ALERT_STATUS_UUID, alertStatusConfig);
  server.addCharacteristic(PASS_SERVICE_UUID, RINGER_SETTING_UUID, ringerSettingConfig);
  server.addCharacteristic(PASS_SERVICE_UUID, RINGER_CONTROL_POINT_UUID, controlConfig);
  server.setValue(PASS_SERVICE_UUID, ALERT_STATUS_UUID, &alertStatus, sizeof(alertStatus));
  server.setValue(PASS_SERVICE_UUID, RINGER_SETTING_UUID, &ringerSetting, sizeof(ringerSetting));

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
  ble.update();
  delay(1);
}
