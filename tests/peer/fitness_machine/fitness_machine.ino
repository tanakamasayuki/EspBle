// fitness_machine DUT: EspBle GATT client for the standard Fitness Machine
// Service (0x1826). It reads Fitness Machine Feature (0x2ACC), subscribes to
// Indoor Bike Data (0x2AD2) and decodes the flags-driven fields, then exercises
// the Fitness Machine Control Point (0x2AD9): Request Control and Set Target
// Power, each answered by a Control Point response indication, with Set Target
// Power also driving a Fitness Machine Status (0x2ADA) notification.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *FTMS_SERVICE_UUID = "1826";
static constexpr const char *INDOOR_BIKE_DATA_UUID = "2ad2";
static constexpr const char *FITNESS_MACHINE_FEATURE_UUID = "2acc";
static constexpr const char *CONTROL_POINT_UUID = "2ad9";
static constexpr const char *STATUS_UUID = "2ada";

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
    if (result.characteristicUuid.equalsIgnoreCase(INDOOR_BIKE_DATA_UUID))
      Serial.printf("FTMS_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
    else if (result.characteristicUuid.equalsIgnoreCase(CONTROL_POINT_UUID))
      Serial.printf("CP_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
    else if (result.characteristicUuid.equalsIgnoreCase(STATUS_UUID))
      Serial.printf("STATUS_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("FTMS_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    const String &value = notification.value;
    if (notification.characteristicUuid.equalsIgnoreCase(INDOOR_BIKE_DATA_UUID))
    {
      const bool valid = value.length() >= 2;
      unsigned flags = 0, speed = 0, cadence = 0;
      int power = 0;
      if (valid)
      {
        flags = u16(value, 0);
        size_t offset = 2;
        if (!(flags & 0x0001)) { speed = u16(value, offset); offset += 2; }
        if (flags & 0x0002) offset += 2;
        if (flags & 0x0004) { cadence = u16(value, offset) / 2; offset += 2; }
        if (flags & 0x0008) offset += 2;
        if (flags & 0x0010) offset += 3;
        if (flags & 0x0020) offset += 2;
        if (flags & 0x0040) { power = static_cast<int16_t>(u16(value, offset)); offset += 2; }
      }
      Serial.printf("FTMS_BIKE valid=%u flags=%04x speed=%u cadence=%u power=%d context=%s\n",
        valid ? 1 : 0, flags, speed, cadence, power, contextName());
    }
    else if (notification.characteristicUuid.equalsIgnoreCase(CONTROL_POINT_UUID))
    {
      const bool valid = value.length() == 3 && static_cast<uint8_t>(value[0]) == 0x80;
      Serial.printf("CP_RESPONSE valid=%u op=%02x result=%u indication=%u context=%s\n",
        valid ? 1 : 0,
        valid ? static_cast<uint8_t>(value[1]) : 0,
        valid ? static_cast<uint8_t>(value[2]) : 0,
        notification.indication ? 1 : 0, contextName());
    }
    else if (notification.characteristicUuid.equalsIgnoreCase(STATUS_UUID))
    {
      const bool valid = value.length() >= 1;
      const uint8_t type = valid ? static_cast<uint8_t>(value[0]) : 0;
      int power = (value.length() >= 3) ? static_cast<int16_t>(u16(value, 1)) : 0;
      Serial.printf("FTMS_STATUS type=%02x power=%d context=%s\n", type, power, contextName());
    }
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
    else if (command == 'c' && connectionId != 0)
    {
      // Subscribe the Control Point (indications) and Status (notifications).
      const bool cp = ble.subscribe(connectionId, FTMS_SERVICE_UUID, CONTROL_POINT_UUID, false);
      const bool st = ble.subscribe(connectionId, FTMS_SERVICE_UUID, STATUS_UUID, true);
      Serial.printf("CONTROL_SUBSCRIBE_REQUESTED cp=%u status=%u\n", cp ? 1 : 0, st ? 1 : 0);
    }
    else if (command == 'r' && connectionId != 0)
    {
      // Request Control (op 0x00).
      const uint8_t request[1] = {0x00};
      const bool accepted = ble.writeCharacteristic(
        connectionId, FTMS_SERVICE_UUID, CONTROL_POINT_UUID, String(reinterpret_cast<const char *>(request), 1), true);
      Serial.println(accepted ? "REQUEST_CONTROL_REQUESTED" : "REQUEST_CONTROL_FAILED");
    }
    else if (command == 'p' && connectionId != 0)
    {
      // Set Target Power (op 0x05) to 250 W (sint16 LE).
      const int16_t power = 250;
      const uint8_t command3[3] = {
        0x05,
        static_cast<uint8_t>(static_cast<uint16_t>(power) & 0xFF),
        static_cast<uint8_t>((static_cast<uint16_t>(power) >> 8) & 0xFF)};
      const bool accepted = ble.writeCharacteristic(
        connectionId, FTMS_SERVICE_UUID, CONTROL_POINT_UUID, String(reinterpret_cast<const char *>(command3), 3), true);
      Serial.println(accepted ? "SET_TARGET_POWER_REQUESTED" : "SET_TARGET_POWER_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
