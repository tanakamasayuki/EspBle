#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *HEART_RATE_SERVICE_UUID = "180d";
static constexpr const char *HEART_RATE_MEASUREMENT_UUID = "2a37";
static constexpr const char *BODY_SENSOR_LOCATION_UUID = "2a38";

EspBle ble;
TaskHandle_t loopTask = nullptr;
uint8_t measurement[] = {0x10, 65, 0x00, 0x04};
const uint8_t bodySensorLocation = 1;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig locationConfig;
  locationConfig.readable = true;
  auto &server = ble.gattServer();
  if (!server.addService(HEART_RATE_SERVICE_UUID) ||
      !server.addCharacteristic(
        HEART_RATE_SERVICE_UUID, HEART_RATE_MEASUREMENT_UUID, measurementConfig) ||
      !server.addCharacteristic(
        HEART_RATE_SERVICE_UUID, BODY_SENSOR_LOCATION_UUID, locationConfig) ||
      !server.setValue(HEART_RATE_SERVICE_UUID, HEART_RATE_MEASUREMENT_UUID,
        measurement, sizeof(measurement)) ||
      !server.setValue(HEART_RATE_SERVICE_UUID, BODY_SENSOR_LOCATION_UUID,
        &bodySensorLocation, 1))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("HEART_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("HEART_SENT success=%u length=%u flags=%02x context=%s\n",
      result.success ? 1 : 0,
      static_cast<unsigned>(result.value.length()),
      result.value.length() > 0 ? static_cast<uint8_t>(result.value[0]) : 0,
      contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Heart Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Heart Peer");
  ble.advertising().addServiceUuid(HEART_RATE_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'h')
    {
      const uint8_t extendedMeasurement[] = {
        0x19,       // 16-bit heart rate, Energy Expended, RR-Interval.
        0x2c, 0x01, // 300 bpm.
        0x2a, 0x00, // 42 kJ.
        0x00, 0x04  // 1024/1024 second.
      };
      const bool stored = ble.gattServer().setValue(
        HEART_RATE_SERVICE_UUID, HEART_RATE_MEASUREMENT_UUID,
        extendedMeasurement, sizeof(extendedMeasurement));
      const bool notified = ble.gattServer().notify(
        HEART_RATE_SERVICE_UUID, HEART_RATE_MEASUREMENT_UUID,
        extendedMeasurement, sizeof(extendedMeasurement));
      Serial.printf("HEART_UPDATED stored=%u notified=%u\n",
        stored ? 1 : 0, notified ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
