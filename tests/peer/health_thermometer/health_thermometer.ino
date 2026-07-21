// health_thermometer DUT: EspBle GATT client for the Health Thermometer
// Service. It reads Temperature Type, subscribes to Temperature Measurement
// indications, and decodes the IEEE-11073 32-bit FLOAT, reporting the
// temperature scaled by 100 so the test can assert an exact value.
#include <EspBle.h>
#include <EspBleMedicalFloat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static constexpr const char *HEALTH_THERMOMETER_SERVICE_UUID = "1809";
static constexpr const char *TEMPERATURE_MEASUREMENT_UUID = "2a1c";
static constexpr const char *TEMPERATURE_TYPE_UUID = "2a1d";

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
      connection.id, HEALTH_THERMOMETER_SERVICE_UUID, TEMPERATURE_TYPE_UUID);
    Serial.println(accepted ? "TYPE_READ_REQUESTED" : "TYPE_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    const bool valid = result.success && result.value.length() == 1;
    Serial.printf("TYPE_READ valid=%u value=%u context=%s\n",
      valid ? 1 : 0, valid ? static_cast<uint8_t>(result.value[0]) : 0, contextName());
    if (valid)
    {
      // Temperature Measurement is an indication (notifications=false).
      const bool accepted = ble.subscribe(
        result.connectionId, HEALTH_THERMOMETER_SERVICE_UUID, TEMPERATURE_MEASUREMENT_UUID, false);
      Serial.println(accepted ? "THERM_SUBSCRIBE_REQUESTED" : "THERM_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("THERM_SUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("THERM_UNSUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(TEMPERATURE_MEASUREMENT_UUID))
      return;
    const bool valid = notification.value.length() >= 5;
    const uint8_t flags = valid ? static_cast<uint8_t>(notification.value[0]) : 0;
    long temperatureHundredths = 0;
    if (valid)
    {
      const uint8_t bytes[4] = {
        static_cast<uint8_t>(notification.value[1]),
        static_cast<uint8_t>(notification.value[2]),
        static_cast<uint8_t>(notification.value[3]),
        static_cast<uint8_t>(notification.value[4])};
      const double temperature = espBleReadMedicalFloat32LE(bytes);
      temperatureHundredths = lround(temperature * 100.0);
    }
    Serial.printf("THERM_MEASUREMENT valid=%u indication=%u flags=%02x temp_x100=%ld context=%s\n",
      valid ? 1 : 0, notification.indication ? 1 : 0, flags, temperatureHundredths, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(HEALTH_THERMOMETER_SERVICE_UUID))
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
      const bool accepted = ble.unsubscribe(
        connectionId, HEALTH_THERMOMETER_SERVICE_UUID, TEMPERATURE_MEASUREMENT_UUID);
      Serial.println(accepted ? "THERM_UNSUBSCRIBE_REQUESTED" : "THERM_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
