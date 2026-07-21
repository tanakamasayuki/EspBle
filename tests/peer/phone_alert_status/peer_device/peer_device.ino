// phone_alert_status peer_device: EspBle GATT server for the standard Phone
// Alert Status Service. Alert Status (0x2A3F) and Ringer Setting (0x2A41) are
// read/notify uint8 values; the Ringer Control Point (0x2A40) is Write Without
// Response. Set Silent Mode (1) switches Ringer Setting to Silent and notifies;
// Cancel Silent Mode (3) switches it back to Normal and notifies.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *PASS_SERVICE_UUID = "180e";
static constexpr const char *ALERT_STATUS_UUID = "2a3f";
static constexpr const char *RINGER_SETTING_UUID = "2a41";
static constexpr const char *RINGER_CONTROL_POINT_UUID = "2a40";

EspBle ble;
TaskHandle_t loopTask = nullptr;
// Alert Status bitmask: bit 0 = Ringer State active.
const uint8_t alertStatus = 0x01;
uint8_t ringerSetting = 1; // 0 = Silent, 1 = Normal

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static void setRingerSetting(uint8_t value)
{
  ringerSetting = value;
  ble.gattServer().setValue(PASS_SERVICE_UUID, RINGER_SETTING_UUID, &ringerSetting, sizeof(ringerSetting));
  ble.gattServer().notify(PASS_SERVICE_UUID, RINGER_SETTING_UUID, &ringerSetting, sizeof(ringerSetting));
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig alertStatusConfig;
  alertStatusConfig.readable = true;
  alertStatusConfig.notifiable = true;
  EspBleGattCharacteristicConfig ringerSettingConfig;
  ringerSettingConfig.readable = true;
  ringerSettingConfig.notifiable = true;
  EspBleGattCharacteristicConfig controlConfig;
  controlConfig.writableWithoutResponse = true;
  auto &server = ble.gattServer();
  if (!server.addService(PASS_SERVICE_UUID) ||
      !server.addCharacteristic(PASS_SERVICE_UUID, ALERT_STATUS_UUID, alertStatusConfig) ||
      !server.addCharacteristic(PASS_SERVICE_UUID, RINGER_SETTING_UUID, ringerSettingConfig) ||
      !server.addCharacteristic(PASS_SERVICE_UUID, RINGER_CONTROL_POINT_UUID, controlConfig) ||
      !server.setValue(PASS_SERVICE_UUID, ALERT_STATUS_UUID, &alertStatus, sizeof(alertStatus)) ||
      !server.setValue(PASS_SERVICE_UUID, RINGER_SETTING_UUID, &ringerSetting, sizeof(ringerSetting)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(RINGER_CONTROL_POINT_UUID) || write.value.length() != 1)
      return;
    const uint8_t command = static_cast<uint8_t>(write.value[0]);
    Serial.printf("CONTROL_WRITE command=%u context=%s\n", command, contextName());
    // 1 = Set Silent Mode, 2 = Mute Once, 3 = Cancel Silent Mode.
    if (command == 1)
      setRingerSetting(0); // Silent
    else if (command == 3)
      setRingerSetting(1); // Normal
  });
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("RINGER_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle PASS Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle PASS Peer");
  ble.advertising().addServiceUuid(PASS_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
