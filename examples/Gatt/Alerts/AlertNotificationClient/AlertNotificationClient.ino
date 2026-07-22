// en: AlertNotificationClient - connect to an Alert Notification Service
//     (0x1811), read Supported New Alert Category, subscribe to New Alert, and
//     write the Control Point "Notify New Alert Immediately" command; then
//     decode the New Alert (category, count, text) it triggers.
// ja: AlertNotificationClient - Alert Notification Service（0x1811）へ接続し、
//     Supported New Alert CategoryをRead、New Alertを購読、Control Pointへ
//     「Notify New Alert Immediately」コマンドをWriteして、発火したNew Alert
//     （category、count、text）をデコードする。
#include <EspBle.h>

static constexpr const char *ANS_SERVICE_UUID = "1811";
static constexpr const char *SUPPORTED_NEW_ALERT_CATEGORY_UUID = "2a47";
static constexpr const char *NEW_ALERT_UUID = "2a46";
static constexpr const char *ALERT_CONTROL_POINT_UUID = "2a44";

EspBle ble;

void setup()
{
  Serial.begin(115200);
  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    ble.readCharacteristic(connection.id, ANS_SERVICE_UUID, SUPPORTED_NEW_ALERT_CATEGORY_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(SUPPORTED_NEW_ALERT_CATEGORY_UUID) &&
        result.success && result.value.length() == 2)
    {
      const uint16_t mask = static_cast<uint8_t>(result.value[0]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(result.value[1])) << 8);
      Serial.printf("Supported categories: 0x%04x\n", mask);
      ble.subscribe(result.connectionId, ANS_SERVICE_UUID, NEW_ALERT_UUID);
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (result.success)
    {
      // en: Notify New Alert Immediately (command 2) for Email (category 1).
      // ja: Email（category 1）に対してNotify New Alert Immediately（command 2）。
      const uint8_t request[2] = {0x02, 0x01};
      ble.writeCharacteristic(
        result.connectionId, ANS_SERVICE_UUID, ALERT_CONTROL_POINT_UUID, request, sizeof(request), true);
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(NEW_ALERT_UUID) ||
        notification.value.length() < 2)
      return;
    const String &value = notification.value;
    const uint8_t category = static_cast<uint8_t>(value[0]);
    const uint8_t count = static_cast<uint8_t>(value[1]);
    Serial.printf("New Alert: category %u, count %u, text \"%s\"\n",
      category, count, value.substring(2).c_str());
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(ANS_SERVICE_UUID))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  ble.update();
  delay(1);
}
