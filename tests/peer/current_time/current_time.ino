#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *CURRENT_TIME_SERVICE_UUID = "1805";
static constexpr const char *CURRENT_TIME_UUID = "2a2b";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static void printTime(const String &value, const char *event)
{
  const bool valid = value.length() == 10;
  const uint16_t year = valid ? static_cast<uint8_t>(value[0]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[1])) << 8) : 0;
  Serial.printf(
    "%s valid=%u year=%u month=%u day=%u hour=%u minute=%u second=%u weekday=%u fraction=%u reason=%02x context=%s\n",
    event, valid ? 1 : 0, year,
    valid ? static_cast<uint8_t>(value[2]) : 0,
    valid ? static_cast<uint8_t>(value[3]) : 0,
    valid ? static_cast<uint8_t>(value[4]) : 0,
    valid ? static_cast<uint8_t>(value[5]) : 0,
    valid ? static_cast<uint8_t>(value[6]) : 0,
    valid ? static_cast<uint8_t>(value[7]) : 0,
    valid ? static_cast<uint8_t>(value[8]) : 0,
    valid ? static_cast<uint8_t>(value[9]) : 0,
    contextName());
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
      connection.id, CURRENT_TIME_SERVICE_UUID, CURRENT_TIME_UUID);
    Serial.println(accepted ? "TIME_READ_REQUESTED" : "TIME_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.printf("TIME_READ_FAILED context=%s\n", contextName());
      return;
    }
    printTime(result.value, "TIME_READ");
    const bool accepted = ble.subscribe(
      result.connectionId, CURRENT_TIME_SERVICE_UUID, CURRENT_TIME_UUID);
    Serial.println(accepted ? "TIME_SUBSCRIBE_REQUESTED" : "TIME_SUBSCRIBE_REQUEST_FAILED");
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("TIME_SUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("TIME_UNSUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.serviceUuid.equalsIgnoreCase(CURRENT_TIME_SERVICE_UUID) &&
        notification.characteristicUuid.equalsIgnoreCase(CURRENT_TIME_UUID))
    {
      printTime(notification.value, "TIME_CHANGED");
    }
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(CURRENT_TIME_SERVICE_UUID)) return;
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
        connectionId, CURRENT_TIME_SERVICE_UUID, CURRENT_TIME_UUID);
      Serial.println(accepted ? "TIME_UNSUBSCRIBE_REQUESTED" : "TIME_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId)
        ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
