#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *ENVIRONMENTAL_SENSING_SERVICE_UUID = "181a";
static constexpr const char *CHARACTERISTIC_UUIDS[] = {"2a6e", "2a6f", "2a6d"};
static constexpr const char *TEMPERATURE_UUID = "2a6e";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;
size_t readIndex = 0;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static uint16_t decode16(const String &value)
{
  return static_cast<uint8_t>(value[0]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[1])) << 8);
}

static uint32_t decode32(const String &value)
{
  return static_cast<uint8_t>(value[0]) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[1])) << 8) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[2])) << 16) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[3])) << 24);
}

static bool requestRead(EspBleConnectionId id)
{
  return ble.readCharacteristic(id, ENVIRONMENTAL_SENSING_SERVICE_UUID,
    CHARACTERISTIC_UUIDS[readIndex]);
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
    readIndex = 0;
    Serial.println(requestRead(connection.id) ? "ENV_READ_REQUESTED" : "ENV_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    bool valid = result.success;
    int32_t raw = 0;
    if (readIndex == 0)
    {
      valid = valid && result.value.length() == 2;
      if (valid) raw = static_cast<int16_t>(decode16(result.value));
    }
    else if (readIndex == 1)
    {
      valid = valid && result.value.length() == 2;
      if (valid) raw = decode16(result.value);
    }
    else
    {
      valid = valid && result.value.length() == 4;
      if (valid) raw = static_cast<int32_t>(decode32(result.value));
    }
    Serial.printf("ENV_READ index=%u valid=%u raw=%ld context=%s\n",
      static_cast<unsigned>(readIndex), valid ? 1 : 0,
      static_cast<long>(raw), contextName());
    if (!valid) return;

    ++readIndex;
    if (readIndex < 3)
    {
      Serial.println(requestRead(result.connectionId)
        ? "ENV_READ_REQUESTED" : "ENV_READ_REQUEST_FAILED");
    }
    else
    {
      const bool accepted = ble.subscribe(
        result.connectionId, ENVIRONMENTAL_SENSING_SERVICE_UUID, TEMPERATURE_UUID);
      Serial.println(accepted ? "ENV_SUBSCRIBE_REQUESTED" : "ENV_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("ENV_SUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("ENV_UNSUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    const bool valid = notification.serviceUuid.equalsIgnoreCase(
        ENVIRONMENTAL_SENSING_SERVICE_UUID) &&
      notification.characteristicUuid.equalsIgnoreCase(TEMPERATURE_UUID) &&
      notification.value.length() == 2;
    const int16_t raw = valid
      ? static_cast<int16_t>(decode16(notification.value)) : 0;
    Serial.printf("ENV_TEMPERATURE_CHANGED valid=%u raw=%d context=%s\n",
      valid ? 1 : 0, raw, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested ||
        !result.advertisesService(ENVIRONMENTAL_SENSING_SERVICE_UUID)) return;
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
        connectionId, ENVIRONMENTAL_SENSING_SERVICE_UUID, TEMPERATURE_UUID);
      Serial.println(accepted ? "ENV_UNSUBSCRIBE_REQUESTED" : "ENV_UNSUBSCRIBE_REQUEST_FAILED");
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
