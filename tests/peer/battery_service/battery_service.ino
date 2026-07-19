#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *BATTERY_SERVICE_UUID = "180f";
static constexpr const char *BATTERY_LEVEL_UUID = "2a19";

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
      connection.id, BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID);
    Serial.println(accepted ? "BATTERY_READ_REQUESTED" : "BATTERY_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    const bool valid = result.success && result.value.length() == 1;
    Serial.printf("BATTERY_READ success=%u length=%u level=%u context=%s\n",
      valid ? 1 : 0,
      static_cast<unsigned>(result.value.length()),
      valid ? static_cast<uint8_t>(result.value[0]) : 0,
      contextName());
    if (valid)
    {
      const bool accepted = ble.subscribe(
        result.connectionId, BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID);
      Serial.println(accepted ? "BATTERY_SUBSCRIBE_REQUESTED" : "BATTERY_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("BATTERY_SUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("BATTERY_UNSUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    const bool valid = notification.serviceUuid.equalsIgnoreCase(BATTERY_SERVICE_UUID) &&
      notification.characteristicUuid.equalsIgnoreCase(BATTERY_LEVEL_UUID) &&
      notification.value.length() == 1;
    Serial.printf("BATTERY_CHANGED valid=%u level=%u context=%s\n",
      valid ? 1 : 0,
      valid ? static_cast<uint8_t>(notification.value[0]) : 0,
      contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(BATTERY_SERVICE_UUID)) return;
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
    else if (command == 'u' && connectionId != 0)
    {
      const bool accepted = ble.unsubscribe(
        connectionId, BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID);
      Serial.println(accepted ? "BATTERY_UNSUBSCRIBE_REQUESTED" : "BATTERY_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
