// fitness_machine DUT: EspBle GATT client for the standard Fitness Machine
// Service (0x1826). It reads Fitness Machine Feature (0x2ACC, uint32 pair),
// subscribes to Indoor Bike Data (0x2AD2) notifications, and decodes the
// flags-driven fields (instantaneous speed, cadence, power).
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *FTMS_SERVICE_UUID = "1826";
static constexpr const char *INDOOR_BIKE_DATA_UUID = "2ad2";
static constexpr const char *FITNESS_MACHINE_FEATURE_UUID = "2acc";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static uint16_t u16(const String &value, size_t offset)
{
  return static_cast<uint8_t>(value[offset]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
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
      connection.id, FTMS_SERVICE_UUID, FITNESS_MACHINE_FEATURE_UUID);
    Serial.println(accepted ? "FEATURE_READ_REQUESTED" : "FEATURE_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    const bool valid = result.success && result.value.length() == 8;
    unsigned long features = 0;
    if (valid)
    {
      features = static_cast<uint8_t>(result.value[0]) |
        (static_cast<uint32_t>(static_cast<uint8_t>(result.value[1])) << 8) |
        (static_cast<uint32_t>(static_cast<uint8_t>(result.value[2])) << 16) |
        (static_cast<uint32_t>(static_cast<uint8_t>(result.value[3])) << 24);
    }
    Serial.printf("FEATURE_READ valid=%u features=%lu context=%s\n",
      valid ? 1 : 0, features, contextName());
    if (valid)
    {
      const bool accepted = ble.subscribe(
        result.connectionId, FTMS_SERVICE_UUID, INDOOR_BIKE_DATA_UUID);
      Serial.println(accepted ? "FTMS_SUBSCRIBE_REQUESTED" : "FTMS_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("FTMS_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("FTMS_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(INDOOR_BIKE_DATA_UUID))
      return;
    const String &value = notification.value;
    const bool valid = value.length() >= 2;
    unsigned flags = 0;
    unsigned speed = 0;   // 0.01 km/h units
    unsigned cadence = 0; // rpm (raw is 0.5/min units)
    int power = 0;        // watts
    if (valid)
    {
      flags = u16(value, 0);
      size_t offset = 2;
      if (!(flags & 0x0001)) { speed = u16(value, offset); offset += 2; } // Instantaneous Speed
      if (flags & 0x0002) offset += 2;                                    // Average Speed
      if (flags & 0x0004) { cadence = u16(value, offset) / 2; offset += 2; } // Instantaneous Cadence
      if (flags & 0x0008) offset += 2;                                    // Average Cadence
      if (flags & 0x0010) offset += 3;                                    // Total Distance
      if (flags & 0x0020) offset += 2;                                    // Resistance Level
      if (flags & 0x0040) { power = static_cast<int16_t>(u16(value, offset)); offset += 2; } // Instantaneous Power
    }
    Serial.printf("FTMS_BIKE valid=%u flags=%04x speed=%u cadence=%u power=%d context=%s\n",
      valid ? 1 : 0, flags, speed, cadence, power, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(FTMS_SERVICE_UUID))
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
      const bool accepted = ble.unsubscribe(connectionId, FTMS_SERVICE_UUID, INDOOR_BIKE_DATA_UUID);
      Serial.println(accepted ? "FTMS_UNSUBSCRIBE_REQUESTED" : "FTMS_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
