// health_thermometer peer_device: EspBle GATT server for the standard Health
// Thermometer Service. Temperature Measurement (0x2A1C) is an indication whose
// value is an IEEE-11073 32-bit FLOAT; Temperature Type (0x2A1D) is a readable
// enum. The DUT reads the type, subscribes to indications, and decodes the
// FLOAT.
#include <EspBle.h>
#include <EspBleMedicalFloat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *HEALTH_THERMOMETER_SERVICE_UUID = "1809";
static constexpr const char *TEMPERATURE_MEASUREMENT_UUID = "2a1c";
static constexpr const char *TEMPERATURE_TYPE_UUID = "2a1d";

EspBle ble;
TaskHandle_t loopTask = nullptr;
const uint8_t temperatureType = 0x02; // Body

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
  measurementConfig.indicatable = true;
  EspBleGattCharacteristicConfig typeConfig;
  typeConfig.readable = true;
  auto &server = ble.gattServer();
  if (!server.addService(HEALTH_THERMOMETER_SERVICE_UUID) ||
      !server.addCharacteristic(
        HEALTH_THERMOMETER_SERVICE_UUID, TEMPERATURE_MEASUREMENT_UUID, measurementConfig) ||
      !server.addCharacteristic(
        HEALTH_THERMOMETER_SERVICE_UUID, TEMPERATURE_TYPE_UUID, typeConfig) ||
      !server.setValue(HEALTH_THERMOMETER_SERVICE_UUID, TEMPERATURE_TYPE_UUID, &temperatureType, 1))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("THERM_SUBSCRIPTION indications=%u context=%s\n",
      subscription.indications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("THERM_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Thermometer Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Thermometer Peer");
  ble.advertising().addServiceUuid(HEALTH_THERMOMETER_SERVICE_UUID);
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
      // flags = 0x00 (Celsius, no timestamp, no type field), then 37.5 degrees
      // as a 32-bit FLOAT (mantissa 375, exponent -1).
      uint8_t measurement[5];
      measurement[0] = 0x00;
      espBleWriteMedicalFloat32LE(&measurement[1], 375, -1);
      const bool stored = ble.gattServer().setValue(
        HEALTH_THERMOMETER_SERVICE_UUID, TEMPERATURE_MEASUREMENT_UUID, measurement, sizeof(measurement));
      const bool indicated = ble.gattServer().indicate(
        HEALTH_THERMOMETER_SERVICE_UUID, TEMPERATURE_MEASUREMENT_UUID, measurement, sizeof(measurement));
      Serial.printf("THERM_UPDATED stored=%u indicated=%u\n", stored ? 1 : 0, indicated ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
