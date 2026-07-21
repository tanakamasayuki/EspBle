// blood_pressure DUT: EspBle GATT client for the Blood Pressure Service. It
// reads Blood Pressure Feature, subscribes to Blood Pressure Measurement
// indications, and decodes the systolic/diastolic/mean SFLOAT values.
#include <EspBle.h>
#include <EspBleMedicalFloat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static constexpr const char *BLOOD_PRESSURE_SERVICE_UUID = "1810";
static constexpr const char *BLOOD_PRESSURE_MEASUREMENT_UUID = "2a35";
static constexpr const char *BLOOD_PRESSURE_FEATURE_UUID = "2a49";

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
      connection.id, BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_FEATURE_UUID);
    Serial.println(accepted ? "FEATURE_READ_REQUESTED" : "FEATURE_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    const bool valid = result.success && result.value.length() == 2;
    const uint16_t value = valid
      ? static_cast<uint16_t>(static_cast<uint8_t>(result.value[0]) |
          (static_cast<uint16_t>(static_cast<uint8_t>(result.value[1])) << 8))
      : 0;
    Serial.printf("FEATURE_READ valid=%u value=%u context=%s\n",
      valid ? 1 : 0, value, contextName());
    if (valid)
    {
      const bool accepted = ble.subscribe(
        result.connectionId, BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_MEASUREMENT_UUID, false);
      Serial.println(accepted ? "BP_SUBSCRIBE_REQUESTED" : "BP_SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("BP_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("BP_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(BLOOD_PRESSURE_MEASUREMENT_UUID))
      return;
    const bool valid = notification.value.length() >= 7;
    const uint8_t flags = valid ? static_cast<uint8_t>(notification.value[0]) : 0;
    long systolic = 0, diastolic = 0, mean = 0;
    if (valid)
    {
      const uint8_t *bytes = reinterpret_cast<const uint8_t *>(notification.value.c_str());
      systolic = lround(espBleReadMedicalSFloatLE(&bytes[1]));
      diastolic = lround(espBleReadMedicalSFloatLE(&bytes[3]));
      mean = lround(espBleReadMedicalSFloatLE(&bytes[5]));
    }
    Serial.printf(
      "BP_MEASUREMENT valid=%u indication=%u flags=%02x systolic=%ld diastolic=%ld mean=%ld context=%s\n",
      valid ? 1 : 0, notification.indication ? 1 : 0, flags, systolic, diastolic, mean, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(BLOOD_PRESSURE_SERVICE_UUID))
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
        connectionId, BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_MEASUREMENT_UUID);
      Serial.println(accepted ? "BP_UNSUBSCRIBE_REQUESTED" : "BP_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
