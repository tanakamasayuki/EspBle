// location_navigation DUT: EspBle GATT client for the Location and Navigation
// Service. It reads LN Feature, subscribes to Location and Speed notifications,
// and decodes the flags-driven layout (Instantaneous Speed + Location lat/lon).
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *LN_SERVICE_UUID = "1819";
static constexpr const char *LOCATION_AND_SPEED_UUID = "2a67";
static constexpr const char *LN_FEATURE_UUID = "2a6a";

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

static int32_t readS32(const String &value, size_t offset)
{
  const uint32_t raw = static_cast<uint32_t>(static_cast<uint8_t>(value[offset])) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 1])) << 8) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 2])) << 16) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 3])) << 24);
  return static_cast<int32_t>(raw);
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
    const bool accepted = ble.readCharacteristic(connection.id, LN_SERVICE_UUID, LN_FEATURE_UUID);
    Serial.println(accepted ? "FEATURE_READ_REQUESTED" : "FEATURE_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    const bool valid = result.success && result.value.length() == 4;
    uint32_t value = 0;
    if (valid)
    {
      for (int i = 3; i >= 0; --i)
        value = (value << 8) | static_cast<uint8_t>(result.value[i]);
    }
    Serial.printf("FEATURE_READ valid=%u value=%u context=%s\n",
      valid ? 1 : 0, static_cast<unsigned>(value), contextName());
    if (valid)
    {
      const bool accepted = ble.subscribe(result.connectionId, LN_SERVICE_UUID, LOCATION_AND_SPEED_UUID);
      Serial.println(accepted ? "LNS_SUBSCRIBE_REQUESTED" : "LNS_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("LNS_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("LNS_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(LOCATION_AND_SPEED_UUID))
      return;
    const String &value = notification.value;
    // flags(2) + Instantaneous Speed(2, bit 0) + Location lat/lon(4+4, bit 2).
    const bool valid = value.length() >= 12;
    uint16_t flags = 0;
    unsigned speed = 0;
    long latitude = 0, longitude = 0;
    if (valid)
    {
      flags = readU16(value, 0);
      speed = readU16(value, 2);       // 1/100 m/s
      latitude = readS32(value, 4);    // 1e-7 degrees
      longitude = readS32(value, 8);   // 1e-7 degrees
    }
    Serial.printf(
      "LNS_MEASUREMENT valid=%u flags=%04x speed=%u lat=%ld lon=%ld context=%s\n",
      valid ? 1 : 0, flags, speed, latitude, longitude, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(LN_SERVICE_UUID))
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
      const bool accepted = ble.unsubscribe(connectionId, LN_SERVICE_UUID, LOCATION_AND_SPEED_UUID);
      Serial.println(accepted ? "LNS_UNSUBSCRIBE_REQUESTED" : "LNS_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
