// cycling_power peer_device: EspBle GATT server for the standard Cycling Power
// Service. Cycling Power Measurement (0x2A63) is a notification whose layout
// starts with 16-bit flags and a SIGNED 16-bit instantaneous power (watts);
// Cycling Power Feature (0x2A65, uint32) and Sensor Location (0x2A5D) are
// readable.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *CYCLING_POWER_SERVICE_UUID = "1818";
static constexpr const char *CYCLING_POWER_MEASUREMENT_UUID = "2a63";
static constexpr const char *CYCLING_POWER_FEATURE_UUID = "2a65";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

EspBle ble;
TaskHandle_t loopTask = nullptr;
const uint8_t feature[4] = {0x0C, 0x00, 0x00, 0x00};
const uint8_t sensorLocation = 6; // Right Crank

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
  EspBleGattCharacteristicConfig readConfig;
  readConfig.readable = true;
  auto &server = ble.gattServer();
  if (!server.addService(CYCLING_POWER_SERVICE_UUID) ||
      !server.addCharacteristic(CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_MEASUREMENT_UUID, measurementConfig) ||
      !server.addCharacteristic(CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_FEATURE_UUID, readConfig) ||
      !server.addCharacteristic(CYCLING_POWER_SERVICE_UUID, SENSOR_LOCATION_UUID, readConfig) ||
      !server.setValue(CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_FEATURE_UUID, feature, sizeof(feature)) ||
      !server.setValue(CYCLING_POWER_SERVICE_UUID, SENSOR_LOCATION_UUID, &sensorLocation, 1))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("CP_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("CP_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle CP Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle CP Peer");
  ble.advertising().addServiceUuid(CYCLING_POWER_SERVICE_UUID);
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
      // 16-bit flags 0x0000 (no optional fields) and a signed instantaneous
      // power of -30 W to exercise signed decoding.
      const int16_t power = -30;
      uint8_t measurement[4];
      measurement[0] = 0x00;
      measurement[1] = 0x00;
      measurement[2] = static_cast<uint8_t>(static_cast<uint16_t>(power) & 0xFF);
      measurement[3] = static_cast<uint8_t>((static_cast<uint16_t>(power) >> 8) & 0xFF);
      const bool stored = ble.gattServer().setValue(
        CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_MEASUREMENT_UUID, measurement, sizeof(measurement));
      const bool notified = ble.gattServer().notify(
        CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_MEASUREMENT_UUID, measurement, sizeof(measurement));
      Serial.printf("CP_UPDATED stored=%u notified=%u\n", stored ? 1 : 0, notified ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
