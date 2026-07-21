// alert_notification peer_device: EspBle GATT server for the standard Alert
// Notification Service. Supported New Alert Category (0x2A47) is a readable
// uint16 bitmask; New Alert (0x2A46) is a notification carrying Category ID +
// count + optional text; the Alert Notification Control Point (0x2A44) is
// writable. On "Notify New Alert Immediately" (command 2), the server notifies
// a New Alert for the requested category.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *ANS_SERVICE_UUID = "1811";
static constexpr const char *SUPPORTED_NEW_ALERT_CATEGORY_UUID = "2a47";
static constexpr const char *NEW_ALERT_UUID = "2a46";
static constexpr const char *ALERT_CONTROL_POINT_UUID = "2a44";

EspBle ble;
TaskHandle_t loopTask = nullptr;
// Supported categories bitmask: bit 1 = Email, bit 5 = SMS/MMS => 0x0022.
const uint8_t supportedCategories[2] = {0x22, 0x00};

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig categoryConfig;
  categoryConfig.readable = true;
  EspBleGattCharacteristicConfig alertConfig;
  alertConfig.notifiable = true;
  EspBleGattCharacteristicConfig controlConfig;
  controlConfig.writable = true;
  auto &server = ble.gattServer();
  if (!server.addService(ANS_SERVICE_UUID) ||
      !server.addCharacteristic(ANS_SERVICE_UUID, SUPPORTED_NEW_ALERT_CATEGORY_UUID, categoryConfig) ||
      !server.addCharacteristic(ANS_SERVICE_UUID, NEW_ALERT_UUID, alertConfig) ||
      !server.addCharacteristic(ANS_SERVICE_UUID, ALERT_CONTROL_POINT_UUID, controlConfig) ||
      !server.setValue(ANS_SERVICE_UUID, SUPPORTED_NEW_ALERT_CATEGORY_UUID,
        supportedCategories, sizeof(supportedCategories)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(ALERT_CONTROL_POINT_UUID))
      return;
    const uint8_t command = write.value.length() >= 1 ? static_cast<uint8_t>(write.value[0]) : 0xFF;
    const uint8_t category = write.value.length() >= 2 ? static_cast<uint8_t>(write.value[1]) : 0xFF;
    Serial.printf("CONTROL_WRITE command=%u category=%u context=%s\n", command, category, contextName());
    // Command 2 = Notify New Alert Immediately.
    if (command == 0x02)
    {
      // New Alert: Category ID + Number of New Alert + Text ("Bob").
      const uint8_t alert[5] = {category, 3, 'B', 'o', 'b'};
      ble.gattServer().notify(ANS_SERVICE_UUID, NEW_ALERT_UUID, alert, sizeof(alert));
    }
  });
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("ALERT_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("ALERT_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Alert Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Alert Peer");
  ble.advertising().addServiceUuid(ANS_SERVICE_UUID);
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
