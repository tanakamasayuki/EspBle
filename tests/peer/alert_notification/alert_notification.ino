// alert_notification DUT: EspBle GATT client for the Alert Notification Service.
// It reads Supported New Alert Category, subscribes to New Alert notifications,
// and writes the Alert Notification Control Point "Notify New Alert Immediately"
// command, then decodes the New Alert (category, count, and text) it triggers.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *ANS_SERVICE_UUID = "1811";
static constexpr const char *SUPPORTED_NEW_ALERT_CATEGORY_UUID = "2a47";
static constexpr const char *NEW_ALERT_UUID = "2a46";
static constexpr const char *ALERT_CONTROL_POINT_UUID = "2a44";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();
  if (!ble.begin())
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    const bool accepted = ble.readCharacteristic(
      connection.id, ANS_SERVICE_UUID, SUPPORTED_NEW_ALERT_CATEGORY_UUID);
    Serial.println(accepted ? "CATEGORY_READ_REQUESTED" : "CATEGORY_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(SUPPORTED_NEW_ALERT_CATEGORY_UUID))
      return;
    const bool valid = result.success && result.value.length() == 2;
    uint16_t mask = 0;
    if (valid)
      mask = static_cast<uint8_t>(result.value[0]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(result.value[1])) << 8);
    Serial.printf("CATEGORY_READ valid=%u mask=%04x context=%s\n", valid ? 1 : 0, mask, contextName());
    if (valid)
    {
      const bool accepted = ble.subscribe(result.connectionId, ANS_SERVICE_UUID, NEW_ALERT_UUID);
      Serial.println(accepted ? "ALERT_SUBSCRIBE_REQUESTED" : "ALERT_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("ALERT_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("ALERT_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(ALERT_CONTROL_POINT_UUID))
      Serial.printf("CONTROL_WRITTEN success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(NEW_ALERT_UUID))
      return;
    const String &value = notification.value;
    const bool valid = value.length() >= 2;
    const uint8_t category = valid ? static_cast<uint8_t>(value[0]) : 0;
    const uint8_t count = valid ? static_cast<uint8_t>(value[1]) : 0;
    const int textLength = value.length() > 2 ? static_cast<int>(value.length() - 2) : 0;
    Serial.printf("NEW_ALERT valid=%u category=%u count=%u text=%.*s context=%s\n",
      valid ? 1 : 0, category, count, textLength, value.c_str() + 2, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(ANS_SERVICE_UUID))
      return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
    Serial.println(connectionRequested ? "CONNECT_REQUESTED" : "CONNECT_REQUEST_FAILED");
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's' && !connectionRequested)
    {
      EspBleScanConfig scan;
      scan.active = true;
      Serial.println(ble.scanner().start(scan) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'c' && connectionId != 0)
    {
      // Command 2 = Notify New Alert Immediately, Category 1 = Email.
      const uint8_t request[2] = {0x02, 0x01};
      const bool accepted = ble.writeCharacteristic(
        connectionId, ANS_SERVICE_UUID, ALERT_CONTROL_POINT_UUID, request, sizeof(request), true);
      Serial.println(accepted ? "CONTROL_WRITE_REQUESTED" : "CONTROL_WRITE_REQUEST_FAILED");
    }
    else if (command == 'u' && connectionId != 0)
    {
      const bool accepted = ble.unsubscribe(connectionId, ANS_SERVICE_UUID, NEW_ALERT_UUID);
      Serial.println(accepted ? "ALERT_UNSUBSCRIBE_REQUESTED" : "ALERT_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
