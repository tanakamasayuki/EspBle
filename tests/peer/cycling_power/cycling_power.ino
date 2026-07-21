// cycling_power DUT: EspBle GATT client for the Cycling Power Service. It reads
// Sensor Location, subscribes to Cycling Power Measurement notifications, and
// decodes the 16-bit flags and the SIGNED 16-bit instantaneous power.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *CYCLING_POWER_SERVICE_UUID = "1818";
static constexpr const char *CYCLING_POWER_MEASUREMENT_UUID = "2a63";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

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
      connection.id, CYCLING_POWER_SERVICE_UUID, SENSOR_LOCATION_UUID);
    Serial.println(accepted ? "LOCATION_READ_REQUESTED" : "LOCATION_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    const bool valid = result.success && result.value.length() == 1;
    Serial.printf("LOCATION_READ valid=%u value=%u context=%s\n",
      valid ? 1 : 0, valid ? static_cast<uint8_t>(result.value[0]) : 0, contextName());
    if (valid)
    {
      const bool accepted = ble.subscribe(
        result.connectionId, CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_MEASUREMENT_UUID);
      Serial.println(accepted ? "CP_SUBSCRIBE_REQUESTED" : "CP_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("CP_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("CP_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(CYCLING_POWER_MEASUREMENT_UUID))
      return;
    const String &value = notification.value;
    const bool valid = value.length() >= 4;
    unsigned flags = 0;
    int power = 0;
    if (valid)
    {
      flags = static_cast<uint8_t>(value[0]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(value[1])) << 8);
      const uint16_t rawPower = static_cast<uint8_t>(value[2]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(value[3])) << 8);
      power = static_cast<int16_t>(rawPower);
    }
    Serial.printf("CP_MEASUREMENT valid=%u flags=%04x power=%d context=%s\n",
      valid ? 1 : 0, flags, power, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(CYCLING_POWER_SERVICE_UUID))
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
      const bool accepted = ble.unsubscribe(connectionId, CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_MEASUREMENT_UUID);
      Serial.println(accepted ? "CP_UNSUBSCRIBE_REQUESTED" : "CP_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
