#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *HEART_RATE_SERVICE_UUID = "180d";
static constexpr const char *HEART_RATE_MEASUREMENT_UUID = "2a37";
static constexpr const char *BODY_SENSOR_LOCATION_UUID = "2a38";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static void printMeasurement(const String &value)
{
  bool valid = value.length() >= 2;
  const uint8_t flags = valid ? static_cast<uint8_t>(value[0]) : 0;
  size_t offset = 1;
  uint16_t heartRate = 0;
  if (valid && (flags & 0x01) != 0)
  {
    valid = offset + 2 <= value.length();
    if (valid)
    {
      heartRate = static_cast<uint8_t>(value[offset]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
      offset += 2;
    }
  }
  else if (valid)
  {
    heartRate = static_cast<uint8_t>(value[offset++]);
  }

  const bool hasEnergy = (flags & 0x08) != 0;
  uint16_t energy = 0;
  if (valid && hasEnergy)
  {
    valid = offset + 2 <= value.length();
    if (valid)
    {
      energy = static_cast<uint8_t>(value[offset]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
      offset += 2;
    }
  }
  size_t rrCount = 0;
  uint16_t firstRr = 0;
  if (valid && (flags & 0x10) != 0)
  {
    valid = ((value.length() - offset) % 2) == 0;
    rrCount = valid ? (value.length() - offset) / 2 : 0;
    if (rrCount > 0)
    {
      firstRr = static_cast<uint8_t>(value[offset]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
    }
  }
  else if (valid)
  {
    valid = offset == value.length();
  }
  Serial.printf(
    "HEART_MEASUREMENT valid=%u flags=%02x bpm=%u energy_present=%u energy=%u rr_count=%u first_rr=%u context=%s\n",
    valid ? 1 : 0, flags, heartRate, hasEnergy ? 1 : 0, energy,
    static_cast<unsigned>(rrCount), firstRr, contextName());
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
      connection.id, HEART_RATE_SERVICE_UUID, BODY_SENSOR_LOCATION_UUID);
    Serial.println(accepted ? "LOCATION_READ_REQUESTED" : "LOCATION_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    const bool valid = result.success && result.value.length() == 1;
    Serial.printf("LOCATION_READ valid=%u value=%u context=%s\n",
      valid ? 1 : 0,
      valid ? static_cast<uint8_t>(result.value[0]) : 0,
      contextName());
    if (valid)
    {
      const bool accepted = ble.subscribe(
        result.connectionId, HEART_RATE_SERVICE_UUID, HEART_RATE_MEASUREMENT_UUID);
      Serial.println(accepted ? "HEART_SUBSCRIBE_REQUESTED" : "HEART_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("HEART_SUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("HEART_UNSUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.serviceUuid.equalsIgnoreCase(HEART_RATE_SERVICE_UUID) &&
        notification.characteristicUuid.equalsIgnoreCase(HEART_RATE_MEASUREMENT_UUID))
    {
      printMeasurement(notification.value);
    }
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(HEART_RATE_SERVICE_UUID)) return;
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
        connectionId, HEART_RATE_SERVICE_UUID, HEART_RATE_MEASUREMENT_UUID);
      Serial.println(accepted ? "HEART_UNSUBSCRIBE_REQUESTED" : "HEART_UNSUBSCRIBE_REQUEST_FAILED");
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
