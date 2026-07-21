// cycling_speed_cadence DUT: EspBle GATT client for the Cycling Speed and
// Cadence Service. It reads Sensor Location, subscribes to CSC Measurement
// notifications, and decodes the multi-field wheel/crank layout.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *CSC_SERVICE_UUID = "1816";
static constexpr const char *CSC_MEASUREMENT_UUID = "2a5b";
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
      connection.id, CSC_SERVICE_UUID, SENSOR_LOCATION_UUID);
    Serial.println(accepted ? "LOCATION_READ_REQUESTED" : "LOCATION_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    const bool valid = result.success && result.value.length() == 1;
    Serial.printf("LOCATION_READ valid=%u value=%u context=%s\n",
      valid ? 1 : 0, valid ? static_cast<uint8_t>(result.value[0]) : 0, contextName());
    if (valid)
    {
      const bool accepted = ble.subscribe(result.connectionId, CSC_SERVICE_UUID, CSC_MEASUREMENT_UUID);
      Serial.println(accepted ? "CSC_SUBSCRIBE_REQUESTED" : "CSC_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("CSC_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("CSC_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(CSC_MEASUREMENT_UUID))
      return;
    const String &value = notification.value;
    const bool valid = value.length() >= 11;
    const uint8_t flags = valid ? static_cast<uint8_t>(value[0]) : 0;
    unsigned long wheel = 0, crank = 0;
    unsigned wheelTime = 0, crankTime = 0;
    if (valid)
    {
      wheel = readU32(value, 1);
      wheelTime = readU16(value, 5);
      crank = readU16(value, 7);
      crankTime = readU16(value, 9);
    }
    Serial.printf(
      "CSC_MEASUREMENT valid=%u flags=%02x wheel=%lu wheel_time=%u crank=%lu crank_time=%u context=%s\n",
      valid ? 1 : 0, flags, wheel, wheelTime, crank, crankTime, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(CSC_SERVICE_UUID))
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
      const bool accepted = ble.unsubscribe(connectionId, CSC_SERVICE_UUID, CSC_MEASUREMENT_UUID);
      Serial.println(accepted ? "CSC_UNSUBSCRIBE_REQUESTED" : "CSC_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
