// en: AlertNotificationServer - standard Alert Notification Service (0x1811).
//     Supported New Alert Category (0x2A47) is a readable uint16 bitmask; New
//     Alert (0x2A46) is notified with Category ID + count + text; the Alert
//     Notification Control Point (0x2A44) is writable. "Notify New Alert
//     Immediately" (command 2) triggers a New Alert notification.
// ja: AlertNotificationServer - 標準Alert Notification Service（0x1811）。
//     Supported New Alert Category（0x2A47）はreadableなuint16 bitmask、New Alert
//     （0x2A46）はCategory ID＋count＋text付きでNotify、Alert Notification Control
//     Point（0x2A44）はwritable。「Notify New Alert Immediately」（command 2）で
//     New AlertのNotificationを発火する。
#include <EspBle.h>

static constexpr const char *ANS_SERVICE_UUID = "1811";
static constexpr const char *SUPPORTED_NEW_ALERT_CATEGORY_UUID = "2a47";
static constexpr const char *NEW_ALERT_UUID = "2a46";
static constexpr const char *ALERT_CONTROL_POINT_UUID = "2a44";

EspBle ble;
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
  server.addService(ANS_SERVICE_UUID);
  server.addCharacteristic(ANS_SERVICE_UUID, SUPPORTED_NEW_ALERT_CATEGORY_UUID, categoryConfig);
  server.addCharacteristic(ANS_SERVICE_UUID, NEW_ALERT_UUID, alertConfig);
  server.addCharacteristic(ANS_SERVICE_UUID, ALERT_CONTROL_POINT_UUID, controlConfig);
  server.setValue(ANS_SERVICE_UUID, SUPPORTED_NEW_ALERT_CATEGORY_UUID,
    supportedCategories, sizeof(supportedCategories));

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
      ble.gattServer().notify(ANS_SERVICE_UUID, NEW_ALERT_UUID, alert, sizeof(alert));
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Alert Notification";
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
  ble.update();
  delay(1);
}
