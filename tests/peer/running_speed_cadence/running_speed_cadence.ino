// running_speed_cadence DUT: EspBle GATT client for the Running Speed and
// Cadence Service. It reads Sensor Location, subscribes to RSC Measurement
// notifications, and decodes the mixed-width speed/cadence/stride/distance
// layout.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *RSC_SERVICE_UUID = "1814";
static constexpr const char *RSC_MEASUREMENT_UUID = "2a53";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static uint16_t readU16(const String &value, size_t offset)
{
  return static_cast<uint8_t>(value[offset]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
}

static uint32_t readU32(const String &value, size_t offset)
{
  return static_cast<uint32_t>(static_cast<uint8_t>(value[offset])) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 1])) << 8) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 2])) << 16) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 3])) << 24);
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
      connection.id, RSC_SERVICE_UUID, SENSOR_LOCATION_UUID);
    Serial.println(accepted ? "LOCATION_READ_REQUESTED" : "LOCATION_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    const bool valid = result.success && result.value.length() == 1;
    Serial.printf("LOCATION_READ valid=%u value=%u context=%s\n",
      valid ? 1 : 0, valid ? static_cast<uint8_t>(result.value[0]) : 0, contextName());
    if (valid)
    {
      const bool accepted = ble.subscribe(result.connectionId, RSC_SERVICE_UUID, RSC_MEASUREMENT_UUID);
      Serial.println(accepted ? "RSC_SUBSCRIBE_REQUESTED" : "RSC_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("RSC_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("RSC_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(RSC_MEASUREMENT_UUID))
      return;
    const String &value = notification.value;
    const bool valid = value.length() >= 4;
    const uint8_t flags = valid ? static_cast<uint8_t>(value[0]) : 0;
    unsigned speed = 0, cadence = 0, stride = 0;
    unsigned long distance = 0;
    if (valid)
    {
      speed = readU16(value, 1);
      cadence = static_cast<uint8_t>(value[3]);
      size_t offset = 4;
      if ((flags & 0x01) && value.length() >= offset + 2)
      {
        stride = readU16(value, offset);
        offset += 2;
      }
      if ((flags & 0x02) && value.length() >= offset + 4)
      {
        distance = readU32(value, offset);
      }
    }
    Serial.printf(
      "RSC_MEASUREMENT valid=%u flags=%02x speed=%u cadence=%u stride=%u distance=%lu context=%s\n",
      valid ? 1 : 0, flags, speed, cadence, stride, distance, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(RSC_SERVICE_UUID))
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
    else if (command == 'u' && connectionId != 0)
    {
      const bool accepted = ble.unsubscribe(connectionId, RSC_SERVICE_UUID, RSC_MEASUREMENT_UUID);
      Serial.println(accepted ? "RSC_UNSUBSCRIBE_REQUESTED" : "RSC_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
