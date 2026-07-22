// en: ImmediateAlertServer - standard Immediate Alert Service (0x1802), the Find
//     Me profile target role. Alert Level (0x2A06) is a single Write Without
//     Response uint8 (0 = No Alert, 1 = Mild, 2 = High). The server reacts to
//     each written level in onWritten.
// ja: ImmediateAlertServer - 標準Immediate Alert Service（0x1802）、Find Me
//     プロファイルのターゲット役。Alert Level（0x2A06）はWrite Without Responseの
//     uint8（0 = No Alert、1 = Mild、2 = High）1つだけ。書かれたlevelごとに
//     onWrittenで反応する。
#include <EspBle.h>

static constexpr const char *IMMEDIATE_ALERT_SERVICE_UUID = "1802";
static constexpr const char *ALERT_LEVEL_UUID = "2a06";

EspBle ble;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig alertLevelConfig;
  alertLevelConfig.writable = true;
  alertLevelConfig.writableWithoutResponse = true;
  auto &server = ble.gattServer();
  server.addService(IMMEDIATE_ALERT_SERVICE_UUID);
  server.addCharacteristic(IMMEDIATE_ALERT_SERVICE_UUID, ALERT_LEVEL_UUID, alertLevelConfig);

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
  ble.update();
  delay(1);
}
